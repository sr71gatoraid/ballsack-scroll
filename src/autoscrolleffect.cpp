// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "autoscrolleffect.h"

#include "applicationexclusions.h"
#include "autoscrollconfig.h"
#include "autoscrollinputdevice.h"
#include "autoscrollvisual.h"
#include "configmigration.h"
#include "glyphstyle.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <input_event.h>
#include <pointer_input.h>
#include <scene/workspacescene.h>
#include <window.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <wayland/surface.h>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <QScopedValueRollback>

namespace AutoScroll {

AutoScrollInputFilter::AutoScrollInputFilter(AutoScrollEffect *effect)
    : KWin::InputEventFilter(KWin::InputFilterOrder::Effects),
      m_effect(effect) {}

bool AutoScrollInputFilter::pointerMotion(KWin::PointerMotionEvent *event) {
  return m_effect->handlePointerMotion(event);
}

bool AutoScrollInputFilter::pointerButton(KWin::PointerButtonEvent *event) {
  return m_effect->handlePointerButton(event);
}

bool AutoScrollInputFilter::pointerAxis(KWin::PointerAxisEvent *event) {
  return m_effect->handlePointerAxis(event);
}

bool AutoScrollInputFilter::keyboardKey(KWin::KeyboardKeyEvent *event) {
  return m_effect->handleKeyboardKey(event);
}

AutoScrollEffect::AutoScrollEffect() {
  const KSharedConfig::Ptr config = KWin::effects->config();
  migrateActivationBehavior(config);
  m_config = std::make_unique<AutoScrollConfig>(config);
  m_visual =
      std::make_unique<AutoScrollVisual>(KWin::effects->scene()->overlayItem());
  reconfigure(ReconfigureAll);

  m_inputDevice = std::make_unique<AutoScrollInputDevice>();
  KWin::input()->addInputDevice(m_inputDevice.get());

  m_inputFilter = std::make_unique<AutoScrollInputFilter>(this);
  KWin::input()->installInputEventFilter(m_inputFilter.get());

  m_scrollTimer.setInterval(8);
  m_scrollTimer.setTimerType(Qt::PreciseTimer);
  connect(&m_scrollTimer, &QTimer::timeout, this,
          &AutoScrollEffect::scrollTick);
  m_holdTimer.setSingleShot(true);
  m_holdTimer.setInterval(220);
  connect(&m_holdTimer, &QTimer::timeout, this,
          &AutoScrollEffect::activatePendingHold);
  connect(KWin::effects, &KWin::EffectsHandler::screenAboutToLock, this,
          &AutoScrollEffect::cancelSession);
  connect(KWin::effects, &KWin::EffectsHandler::screenLockingChanged, this,
          [this](bool locked) {
            if (locked) {
              cancelSession();
            }
          });
  connect(KWin::effects,
          &KWin::EffectsHandler::hasActiveFullScreenEffectChanged, this,
          [this]() {
            if (KWin::effects->hasActiveFullScreenEffect()) {
              cancelSession();
            }
          });
}

AutoScrollEffect::~AutoScrollEffect() {
  cancelSession();
  if (m_inputFilter) {
    KWin::input()->uninstallInputEventFilter(m_inputFilter.get());
    m_inputFilter.reset();
  }
  if (m_inputDevice) {
    KWin::input()->removeInputDevice(m_inputDevice.get());
    m_inputDevice.reset();
  }
}

bool AutoScrollEffect::isActive() const { return m_session.isActive(); }

QString AutoScrollEffect::debug(const QString &) const {
  const QPointF pointer = KWin::input()->globalPointer();
  const QPointF visiblePointer = KWin::Cursors::self()->mouse()->pos();
  const QPointF clientPointer = KWin::waylandServer()->seat()->pointerPos();
  return QString::fromUtf8(QJsonDocument(QJsonObject{
      {QStringLiteral("revision"), QStringLiteral("smooth-autoscroll-0.1.0")},
      {QStringLiteral("active"), m_session.isActive()},
      {QStringLiteral("glyphSize"), m_visual->logicalSize()},
      {QStringLiteral("pointerX"), pointer.x()},
      {QStringLiteral("pointerY"), pointer.y()},
      {QStringLiteral("visiblePointerX"), visiblePointer.x()},
      {QStringLiteral("visiblePointerY"), visiblePointer.y()},
      {QStringLiteral("clientPointerX"), clientPointer.x()},
      {QStringLiteral("clientPointerY"), clientPointer.y()},
      {QStringLiteral("anchorX"), m_anchorPosition.x()},
      {QStringLiteral("anchorY"), m_anchorPosition.y()},
  }).toJson(QJsonDocument::Compact));
}

void AutoScrollEffect::reconfigure(ReconfigureFlags) {
  const ActivationMode previousActivationMode = m_activationMode;
  m_config->read();
  m_activationMode = configuredActivationMode();
  m_excludedApplications =
      normalizedApplicationExclusions(m_config->excludedApplications());

  if ((m_session.isActive() || m_pendingPress) &&
      (previousActivationMode != m_activationMode ||
       isWindowExcluded(m_targetWindow) || isWindowExcluded(m_pendingWindow))) {
    cancelSession();
  }

  m_engine.setSettings({
      .deadZone = static_cast<qreal>(m_config->deadZone()),
      .maximumSpeed = static_cast<qreal>(m_config->maximumSpeed()),
      .accelerationExponent = m_config->accelerationExponent(),
      .horizontalScrolling = m_config->horizontalScrolling(),
  });
  m_visual->setAppearance(m_config->glyphStyle(), anchorSize());

  const bool visualFeedback = m_config->visualFeedback();
  if (m_visualFeedback != visualFeedback) {
    m_visualFeedback = visualFeedback;
    if (m_session.isActive()) {
      if (m_visualFeedback) {
        m_visual->show(m_anchorPosition, cursorScale());
      } else {
        m_visual->hide();
      }
    }
  }
}

bool AutoScrollEffect::handlePointerMotion(KWin::PointerMotionEvent *event) {
  if (m_pendingPress) {
    if (m_engine.directionForOffset(event->position - m_pendingPress->position)
        != Direction::Center) {
      activatePendingHold();
    } else if (KWin::input()->findToplevel(event->position) != m_pendingWindow ||
               KWin::input()->pointer()->decoration()) {
      clearPendingClick();
    }
  }
  if (!m_session.isActive()) {
    return false;
  }

  updateSessionModifiers(event->modifiers);
  m_cursorPosition = event->position;
  if (!isStillOnTarget()) {
    cancelSession();
    return false;
  }

  m_session.handleMotion(
      m_engine.directionForOffset(m_cursorPosition - m_anchorPosition) !=
      Direction::Center);
  updateVisual();
  // KWin already updated the real cursor position before this filter. Keep
  // that free movement, but retain the client's input position at the anchor.
  // Otherwise wheels follow whichever sidebar/widget the mouse moves over.
  return true;
}

bool AutoScrollEffect::handlePointerButton(KWin::PointerButtonEvent *event) {
  if (m_replayingClick || event->device == m_inputDevice.get()) {
    return false;
  }

  const bool pressed = event->state == KWin::PointerButtonState::Pressed;
  if (!pressed && event->button == Qt::MiddleButton && m_suppressPendingRelease) {
    m_suppressPendingRelease = false;
    return true;
  }
  if (m_pendingPress) {
    if (!pressed && event->button == Qt::MiddleButton) {
      return replayPendingClick(event);
    }
    if (pressed) {
      clearPendingClick();
    }
  }
  updateSessionModifiers(event->modifiers);
  const bool wasScrollReady = m_session.isScrollReady();
  const InputDecision decision = m_session.handleButton(event->button, pressed);
  if (!wasScrollReady && m_session.isScrollReady()) {
    startScrolling();
  }
  if (decision.cancel) {
    cancelSession();
  }
  if (decision.consume) {
    return true;
  }

  const Qt::KeyboardModifier activationModifier =
      configuredActivationModifier();
  if (pressed && event->button == Qt::MiddleButton &&
      activationModifiersMatch(event->modifiers, activationModifier)) {
    KWin::Window *window = KWin::input()->findToplevel(event->position);
    if (canActivate(window)) {
      if (m_activationMode == ActivationMode::PreserveClicks) {
        m_pendingPress = std::make_unique<KWin::PointerButtonEvent>(*event);
        m_pendingWindow = window;
        m_holdTimer.start();
      } else {
        activate(window, event->position, activationModifier, m_activationMode);
      }
      return true;
    }
  }

  return false;
}

bool AutoScrollEffect::handlePointerAxis(KWin::PointerAxisEvent *event) {
  if (event->device == m_inputDevice.get()) {
    return false;
  }
  if (event->delta == 0.0 && event->deltaV120 == 0) {
    return false;
  }
  clearPendingClick();

  const InputDecision decision = m_session.handleAxis();
  if (decision.cancel) {
    cancelSession();
  }
  return decision.consume;
}

bool AutoScrollEffect::handleKeyboardKey(KWin::KeyboardKeyEvent *event) {
  updateSessionModifiers(event->modifiers);

  if (event->key != Qt::Key_Escape) {
    return false;
  }
  clearPendingClick();

  const bool pressed = event->state != KWin::KeyboardKeyState::Released;
  const InputDecision decision = m_session.handleEscape(pressed);
  if (decision.cancel) {
    cancelSession();
  }
  return decision.consume;
}

Qt::KeyboardModifier AutoScrollEffect::configuredActivationModifier() const {
  switch (m_config->activationModifier()) {
  case AutoScrollConfig::EnumActivationModifier::ControlModifier:
    return Qt::ControlModifier;
  case AutoScrollConfig::EnumActivationModifier::MetaModifier:
    return Qt::MetaModifier;
  case AutoScrollConfig::EnumActivationModifier::AltModifier:
    return Qt::AltModifier;
  case AutoScrollConfig::EnumActivationModifier::ShiftModifier:
    return Qt::ShiftModifier;
  case AutoScrollConfig::EnumActivationModifier::NoModifier:
  default:
    return Qt::NoModifier;
  }
}

ActivationMode AutoScrollEffect::configuredActivationMode() const {
  switch (m_config->activationBehavior()) {
  case AutoScrollConfig::EnumActivationBehavior::Hold:
    return ActivationMode::Hold;
  case AutoScrollConfig::EnumActivationBehavior::Combined:
    return ActivationMode::Combined;
  case AutoScrollConfig::EnumActivationBehavior::PreserveClicks:
    return ActivationMode::PreserveClicks;
  case AutoScrollConfig::EnumActivationBehavior::Toggle:
  default:
    return ActivationMode::Toggle;
  }
}

bool AutoScrollEffect::canActivate(KWin::Window *window) const {
  const bool excludedSurface =
      window &&
      (window->isDeleted() || window->isDesktop() || window->isDock() ||
       window->isInternal() || window->isLockScreen() ||
       window->isLockScreenOverlay() || window->isInputMethod() ||
       window->isOutline() || window->isPopupWindow() || window->isMenu() ||
       window->isDropdownMenu() || window->isPopupMenu() ||
       window->isTooltip() || window->isNotification() ||
       window->isCriticalNotification() || window->isAppletPopup() ||
       window->isOnScreenDisplay());

  return AutoScroll::canActivate({
      .hasWindow = window != nullptr,
      .clientWindow = window && window->isClient(),
      .excludedSurface = excludedSurface,
      .screenLocked = KWin::effects->isScreenLocked(),
      .fullscreenEffectActive = KWin::effects->hasActiveFullScreenEffect(),
      .pointerConstrained = KWin::input()->pointer()->isConstrained(),
      .overDecoration = KWin::input()->pointer()->decoration() != nullptr,
      .excludedApplication = isWindowExcluded(window),
  });
}

bool AutoScrollEffect::isWindowExcluded(KWin::Window *window) const {
  if (!window) {
    return false;
  }
  return isApplicationExcluded({.desktopFileId = window->desktopFileName(),
                                .resourceClass = window->resourceClass()},
                               m_excludedApplications);
}

bool AutoScrollEffect::isStillOnTarget() const {
  return m_targetWindow && !m_targetWindow->isDeleted() &&
         !m_targetWindow->isMinimized() && !m_targetWindow->isHidden() &&
         m_targetWindow->isOnCurrentDesktop() &&
         m_targetWindow->isOnCurrentActivity() && m_targetSurface &&
         KWin::waylandServer()->seat()->focusedPointerSurface() == m_targetSurface;
}

void AutoScrollEffect::activate(KWin::Window *window, const QPointF &position,
                                Qt::KeyboardModifier activationModifier,
                                ActivationMode activationMode) {
  m_session.activate(activationModifier, activationMode);
  m_targetWindow = window;
  auto *seat = KWin::waylandServer()->seat();
  m_targetSurface = seat->focusedPointerSurface();
  m_anchorPosition = position;
  m_cursorPosition = position;
  m_engine.reset();
  seat->setTimestamp(currentTimestamp());
  seat->notifyPointerMotion(position);
  seat->notifyPointerFrame();

  m_targetDestroyedConnection = connect(window, &QObject::destroyed, this,
                                        &AutoScrollEffect::cancelSession);

  if (m_visualFeedback) {
    m_visual->setAppearance(m_config->glyphStyle(), anchorSize());
    m_visual->show(m_anchorPosition, cursorScale());
  }

  if (m_session.isScrollReady()) {
    startScrolling();
  }
  KWin::effects->addRepaintFull();
}

void AutoScrollEffect::clearPendingClick(bool suppressRelease) {
  m_holdTimer.stop();
  if (m_pendingPress && suppressRelease) {
    m_suppressPendingRelease = true;
  }
  m_pendingPress.reset();
  m_pendingWindow.clear();
}

void AutoScrollEffect::activatePendingHold() {
  if (!m_pendingPress) {
    return;
  }
  const QPointF position = KWin::input()->globalPointer();
  if (!canActivate(m_pendingWindow) ||
      !KWin::waylandServer()->seat()->focusedPointerSurface() ||
      !(KWin::input()->qtButtonStates() & Qt::MiddleButton) ||
      !activationModifiersMatch(KWin::input()->keyboardModifiers(),
                                configuredActivationModifier())) {
    clearPendingClick();
    return;
  }
  KWin::Window *window = m_pendingWindow;
  const QPointF anchor = m_pendingPress->position;
  clearPendingClick(false);
  activate(window, anchor, configuredActivationModifier(), ActivationMode::Hold);
  m_cursorPosition = position;
  updateVisual();
}

bool AutoScrollEffect::replayPendingClick(KWin::PointerButtonEvent *release) {
  // Wayland does not report whether a widget handled a click. Defer only the
  // middle-button pair until it is known to be a click rather than a hold.
  // Replay through KWin's normal filters so tab closing, links, paste and
  // configured window actions retain their native behavior.
  KWin::PointerButtonEvent press = *m_pendingPress;
  const bool deliver = canActivate(m_pendingWindow) &&
      KWin::input()->findToplevel(release->position) == m_pendingWindow;
  clearPendingClick(false);
  if (deliver) {
    QScopedValueRollback<bool> guard(m_replayingClick, true);
    KWin::input()->processFilters(&KWin::InputEventFilter::pointerButton, &press);
    KWin::input()->processFilters(&KWin::InputEventFilter::pointerFrame);
    KWin::input()->processFilters(&KWin::InputEventFilter::pointerButton, release);
    KWin::input()->processFilters(&KWin::InputEventFilter::pointerFrame);
  }
  return true;
}

void AutoScrollEffect::updateSessionModifiers(Qt::KeyboardModifiers modifiers) {
  const bool wasScrollReady = m_session.isScrollReady();
  m_session.handleModifiers(modifiers);
  if (!wasScrollReady && m_session.isScrollReady()) {
    startScrolling();
  }
}

void AutoScrollEffect::startScrolling() {
  if (!m_session.isScrollReady() || m_scrollTimer.isActive()) {
    return;
  }
  m_elapsedTimer.start();
  m_scrollTimer.start();
}

void AutoScrollEffect::cancelSession() {
  clearPendingClick();
  const bool wasActive = m_session.cancel();
  if (!wasActive && (!m_visual || !m_visual->isVisible())) {
    return;
  }

  m_scrollTimer.stop();
  if (wasActive && m_inputDevice) {
    emitFrame(m_engine.stop());
  } else {
    m_engine.reset();
  }

  // Resume normal hover/input at the actual pointer position. There is no
  // cursor warp; only the client's paused motion stream needs catching up.
  auto *seat = KWin::waylandServer()->seat();
  if (wasActive && m_targetSurface && seat->focusedPointerSurface() == m_targetSurface) {
    seat->setTimestamp(currentTimestamp());
    seat->notifyPointerMotion(KWin::input()->globalPointer());
    seat->notifyPointerFrame();
  }

  if (m_targetDestroyedConnection) {
    disconnect(m_targetDestroyedConnection);
    m_targetDestroyedConnection = {};
  }
  m_targetWindow.clear();
  m_targetSurface.clear();

  if (m_visual) {
    m_visual->hide();
  }
  KWin::effects->addRepaintFull();
}

void AutoScrollEffect::scrollTick() {
  if (!m_session.isScrollReady() || !isStillOnTarget()) {
    cancelSession();
    return;
  }

  const qreal elapsedSeconds = m_elapsedTimer.nsecsElapsed() / 1'000'000'000.0;
  m_elapsedTimer.restart();
  const ScrollFrame frame =
      m_engine.advance(m_cursorPosition - m_anchorPosition, elapsedSeconds);
  emitFrame(frame);
}

void AutoScrollEffect::emitFrame(const ScrollFrame &frame) {
  bool emitted = false;
  const auto timestamp = currentTimestamp();

  // A release updates KWin's physical button state before this callback.
  // Emit end-of-scroll directly before any synthetic input updates focus,
  // so the stop cannot accidentally be delivered to a different window.
  if (!frame.horizontalActive && !frame.verticalActive &&
      (frame.horizontalStopped || frame.verticalStopped)) {
    auto *seat = KWin::waylandServer()->seat();
    if (m_targetSurface && seat->focusedPointerSurface() == m_targetSurface) {
      seat->setTimestamp(timestamp);
      if (frame.horizontalStopped)
        seat->notifyPointerAxis(Qt::Horizontal, 0.0, 0,
                               KWin::PointerAxisSource::Continuous, false);
      if (frame.verticalStopped)
        seat->notifyPointerAxis(Qt::Vertical, 0.0, 0,
                               KWin::PointerAxisSource::Continuous, false);
      seat->notifyPointerFrame();
    }
    return;
  }

  if (frame.horizontalActive || frame.horizontalStopped) {
    Q_EMIT m_inputDevice->pointerAxisChanged(
        KWin::PointerAxis::Horizontal,
        frame.horizontalActive ? frame.horizontalDelta : 0.0, 0,
        KWin::PointerAxisSource::Continuous, false, timestamp,
        m_inputDevice.get());
    emitted = true;
  }
  if (frame.verticalActive || frame.verticalStopped) {
    Q_EMIT m_inputDevice->pointerAxisChanged(
        KWin::PointerAxis::Vertical,
        frame.verticalActive ? frame.verticalDelta : 0.0, 0,
        KWin::PointerAxisSource::Continuous, false, timestamp,
        m_inputDevice.get());
    emitted = true;
  }
  if (emitted) {
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
  }
}

void AutoScrollEffect::updateVisual() {
  if (!m_visualFeedback || !m_visual->isVisible()) {
    return;
  }
  m_visual->setAppearance(m_config->glyphStyle(), anchorSize());
  m_visual->update(cursorScale());
}

qreal AutoScrollEffect::anchorSize() const {
  const QPointF position = m_session.isActive() ? m_anchorPosition
                                               : KWin::input()->globalPointer();
  if (auto *output = KWin::effects->screenAt(position.toPoint())) {
    return screenRelativeGlyphSize(m_config->glyphSize(), output->geometry().size());
  }
  return normalizedGlyphSize(m_config->glyphSize());
}

qreal AutoScrollEffect::cursorScale() const {
  const QPointF position = m_session.isActive() ? m_anchorPosition
                                               : KWin::input()->globalPointer();
  if (auto *output = KWin::effects->screenAt(position.toPoint())) {
    return output->scale();
  }
  return 1.0;
}

std::chrono::microseconds AutoScrollEffect::currentTimestamp() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
}

} // namespace AutoScroll
