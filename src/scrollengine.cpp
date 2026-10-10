// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scrollengine.h"

#include <algorithm>
#include <cmath>

namespace AutoScroll {
namespace {
// Integrate the exponential response exactly so the feel is independent of
// timer frequency. Stopping in the dead zone and reversing never coast.
qreal smoothDelta(qreal target, qreal &velocity, qreal seconds) {
  constexpr qreal responseSeconds = 0.060;
  if (target == 0.0) {
    velocity = 0.0;
    return 0.0;
  }
  if (target * velocity < 0.0) {
    velocity = 0.0;
  }
  const qreal decay = std::exp(-seconds / responseSeconds);
  const qreal delta = target * seconds +
      (velocity - target) * responseSeconds * (1.0 - decay);
  velocity = target + (velocity - target) * decay;
  return delta;
}
} // namespace


void ScrollEngine::setSettings(const ScrollSettings &settings) {
  m_settings.deadZone = std::clamp(settings.deadZone, 0.0, 256.0);
  m_settings.maximumSpeed = std::clamp(settings.maximumSpeed, 0.0, 10000.0);
  m_settings.accelerationExponent =
      std::clamp(settings.accelerationExponent, 0.1, 8.0);
  m_settings.horizontalScrolling = settings.horizontalScrolling;
}

const ScrollSettings &ScrollEngine::settings() const { return m_settings; }

qreal ScrollEngine::speedForOffset(qreal offset) const {
  const qreal magnitude = std::abs(offset);
  if (magnitude <= m_settings.deadZone ||
      qFuzzyIsNull(m_settings.maximumSpeed)) {
    return 0.0;
  }

  const qreal excess = magnitude - m_settings.deadZone;
  const qreal normalized = std::clamp(excess / FullSpeedDistance, 0.0, 1.0);
  const qreal speed = m_settings.maximumSpeed *
                      std::pow(normalized, m_settings.accelerationExponent);
  return std::copysign(speed, offset);
}

ScrollFrame ScrollEngine::advance(const QPointF &offset, qreal elapsedSeconds) {
  const qreal boundedElapsed = std::clamp(elapsedSeconds, 0.0, 0.1);
  // One circular dead zone and one distance-based speed. Project that speed
  // onto both axes, so a small turn adds a vertical component immediately
  // even after starting horizontally on a vertical-only document.
  const qreal x = m_settings.horizontalScrolling ? offset.x() : 0.0;
  const qreal radius = std::hypot(x, offset.y());
  const qreal speed = speedForOffset(radius);
  const qreal horizontalSpeed = radius > 0.0 ? speed * x / radius : 0.0;
  const qreal verticalSpeed = radius > 0.0 ? speed * offset.y() / radius : 0.0;

  ScrollFrame frame;
  frame.horizontalActive = !qFuzzyIsNull(horizontalSpeed);
  frame.verticalActive = !qFuzzyIsNull(verticalSpeed);
  frame.horizontalStopped = m_horizontalWasActive && !frame.horizontalActive;
  frame.verticalStopped = m_verticalWasActive && !frame.verticalActive;
  frame.horizontalDelta = smoothDelta(horizontalSpeed, m_horizontalSpeed, boundedElapsed);
  frame.verticalDelta = smoothDelta(verticalSpeed, m_verticalSpeed, boundedElapsed);

  m_horizontalWasActive = frame.horizontalActive;
  m_verticalWasActive = frame.verticalActive;
  return frame;
}

ScrollFrame ScrollEngine::stop() {
  ScrollFrame frame;
  frame.horizontalStopped = m_horizontalWasActive;
  frame.verticalStopped = m_verticalWasActive;
  reset();
  return frame;
}

void ScrollEngine::reset() {
  m_horizontalSpeed = 0.0;
  m_verticalSpeed = 0.0;
  m_horizontalWasActive = false;
  m_verticalWasActive = false;
}

Direction ScrollEngine::directionForOffset(const QPointF &offset) const {
  const qreal x = m_settings.horizontalScrolling ? offset.x() : 0.0;
  if (std::hypot(x, offset.y()) <= m_settings.deadZone) return Direction::Center;
  const int horizontal = qFuzzyIsNull(x) ? 0 : (x > 0 ? 1 : -1);
  const int vertical = qFuzzyIsNull(offset.y()) ? 0 : (offset.y() > 0 ? 1 : -1);

  if (horizontal == 0 && vertical == 0) {
    return Direction::Center;
  }
  if (horizontal == 0) {
    return vertical < 0 ? Direction::North : Direction::South;
  }
  if (vertical == 0) {
    return horizontal < 0 ? Direction::West : Direction::East;
  }
  if (horizontal > 0) {
    return vertical < 0 ? Direction::NorthEast : Direction::SouthEast;
  }
  return vertical < 0 ? Direction::NorthWest : Direction::SouthWest;
}

} // namespace AutoScroll
