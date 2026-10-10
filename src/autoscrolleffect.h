// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "scrollengine.h"
#include "sessionstate.h"

#include <effect/effect.h>
#include <input.h>

#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace KWin {
class InputDevice;
class Window;
class SurfaceInterface;
struct KeyboardKeyEvent;
struct PointerAxisEvent;
struct PointerButtonEvent;
struct PointerMotionEvent;
} // namespace KWin

namespace AutoScroll {

class AutoScrollInputDevice;
class AutoScrollVisual;
class AutoScrollConfig;
class AutoScrollEffect;

class AutoScrollInputFilter : public KWin::InputEventFilter {
public:
  explicit AutoScrollInputFilter(AutoScrollEffect *effect);

  bool pointerMotion(KWin::PointerMotionEvent *event) override;
  bool pointerButton(KWin::PointerButtonEvent *event) override;
  bool pointerAxis(KWin::PointerAxisEvent *event) override;
  bool keyboardKey(KWin::KeyboardKeyEvent *event) override;

private:
  AutoScrollEffect *m_effect;
};

class AutoScrollEffect : public KWin::Effect {
  Q_OBJECT

public:
  AutoScrollEffect();
  ~AutoScrollEffect() override;

  bool isActive() const override;
  QString debug(const QString &parameter) const override;
  void reconfigure(ReconfigureFlags flags) override;

  bool handlePointerMotion(KWin::PointerMotionEvent *event);
  bool handlePointerButton(KWin::PointerButtonEvent *event);
  bool handlePointerAxis(KWin::PointerAxisEvent *event);
  bool handleKeyboardKey(KWin::KeyboardKeyEvent *event);

private:
  Qt::KeyboardModifier configuredActivationModifier() const;
  ActivationMode configuredActivationMode() const;
  bool canActivate(KWin::Window *window) const;
  bool isWindowExcluded(KWin::Window *window) const;
  bool isStillOnTarget() const;
  void activate(KWin::Window *window, const QPointF &position,
                Qt::KeyboardModifier activationModifier,
                ActivationMode activationMode);
  void activatePendingHold();
  void clearPendingClick(bool suppressRelease = true);
  bool replayPendingClick(KWin::PointerButtonEvent *release);
  void updateSessionModifiers(Qt::KeyboardModifiers modifiers);
  void startScrolling();
  void cancelSession();
  void scrollTick();
  void emitFrame(const ScrollFrame &frame);
  void updateVisual();
  qreal anchorSize() const;
  qreal cursorScale() const;
  static std::chrono::microseconds currentTimestamp();

  ScrollEngine m_engine;
  SessionState m_session;
  std::unique_ptr<AutoScrollConfig> m_config;
  std::unique_ptr<AutoScrollInputDevice> m_inputDevice;
  std::unique_ptr<AutoScrollInputFilter> m_inputFilter;
  std::unique_ptr<AutoScrollVisual> m_visual;
  QPointer<KWin::Window> m_targetWindow;
  QPointer<KWin::SurfaceInterface> m_targetSurface;
  QMetaObject::Connection m_targetDestroyedConnection;
  QPointF m_anchorPosition;
  QPointF m_cursorPosition;
  QTimer m_scrollTimer;
  QTimer m_holdTimer;
  std::unique_ptr<KWin::PointerButtonEvent> m_pendingPress;
  QPointer<KWin::Window> m_pendingWindow;
  bool m_replayingClick = false;
  bool m_suppressPendingRelease = false;
  QElapsedTimer m_elapsedTimer;
  bool m_visualFeedback = true;
  ActivationMode m_activationMode = ActivationMode::PreserveClicks;
  QStringList m_excludedApplications;
};

} // namespace AutoScroll
