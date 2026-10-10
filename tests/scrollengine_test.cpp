// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scrollengine.h"

#include <QTest>

#include <cmath>

using namespace AutoScroll;

class ScrollEngineTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void deadZoneProducesNoMovement();
  void directionAndMagnitude();
  void maximumSpeedIsCapped();
  void elapsedTimeScalesDelta();
  void speedRampsSmoothly();
  void reversalHasNoWrongWayCoasting();
  void resetClearsVelocity();
  void diagonalAxesAreIndependent();
  void horizontalCanBeDisabled();
  void axisStopIsEmittedOnce();
  void directionClassification();
  void lowSpeedConfigurationRemainsActive();
  void smallTurnFromHorizontalStartsVertical();
  void circularDeadZoneHasNoAxisBands();
  void diagonalSpeedIsCappedAsVector();
};

void ScrollEngineTest::deadZoneProducesNoMovement() {
  ScrollEngine engine;
  const ScrollFrame frame = engine.advance(QPointF(16, -16), 0.01);
  QVERIFY(!frame.horizontalActive);
  QVERIFY(!frame.verticalActive);
  QCOMPARE(frame.horizontalDelta, 0.0);
  QCOMPARE(frame.verticalDelta, 0.0);
}

void ScrollEngineTest::smallTurnFromHorizontalStartsVertical() {
  ScrollEngine engine;
  for (int i = 0; i < 50; ++i)
    engine.advance(QPointF(200, 0), 0.008);
  const auto turned = engine.advance(QPointF(200, 12), 0.008);
  QVERIFY(turned.horizontalActive);
  QVERIFY(turned.verticalActive);
  QVERIFY(turned.verticalDelta > 0);
  QVERIFY(turned.horizontalDelta > turned.verticalDelta);
}

void ScrollEngineTest::circularDeadZoneHasNoAxisBands() {
  ScrollEngine engine;
  QVERIFY(!engine.advance(QPointF(24, 0), 0.008).horizontalActive);
  const auto frame = engine.advance(QPointF(20, 20), 0.008);
  QVERIFY(frame.horizontalActive);
  QVERIFY(frame.verticalActive);
  QCOMPARE(frame.horizontalDelta, frame.verticalDelta);
}

void ScrollEngineTest::diagonalSpeedIsCappedAsVector() {
  ScrollEngine engine;
  ScrollFrame frame;
  for (int i = 0; i < 250; ++i)
    frame = engine.advance(QPointF(10000, 10000), 0.008);
  const qreal speed = std::hypot(frame.horizontalDelta, frame.verticalDelta) / 0.008;
  QVERIFY(speed <= engine.settings().maximumSpeed + 1e-8);
  QVERIFY(speed > engine.settings().maximumSpeed - 1e-6);
}

void ScrollEngineTest::directionAndMagnitude() {
  ScrollEngine engine;
  const qreal positive = engine.speedForOffset(106);
  const qreal negative = engine.speedForOffset(-106);
  QVERIFY(positive > 0);
  QCOMPARE(negative, -positive);
  QVERIFY(positive < engine.settings().maximumSpeed);
}

void ScrollEngineTest::maximumSpeedIsCapped() {
  ScrollEngine engine;
  QCOMPARE(engine.speedForOffset(10000), 800.0);
  QCOMPARE(engine.speedForOffset(-10000), -800.0);
}

void ScrollEngineTest::elapsedTimeScalesDelta() {
  // A constant offset over one second must travel the same distance at
  // 50 Hz and 125 Hz. This catches timer-dependent smoothing.
  ScrollEngine slow;
  ScrollEngine fast;
  qreal slowDistance = 0;
  qreal fastDistance = 0;
  for (int i = 0; i < 50; ++i)
    slowDistance += slow.advance(QPointF(0, 100), 0.020).verticalDelta;
  for (int i = 0; i < 125; ++i)
    fastDistance += fast.advance(QPointF(0, 100), 0.008).verticalDelta;
  QVERIFY(std::abs(slowDistance - fastDistance) < 1e-8);
  QVERIFY(slowDistance > 0);
}

void ScrollEngineTest::speedRampsSmoothly() {
  ScrollEngine engine;
  const qreal first = engine.advance(QPointF(0, 160), 0.008).verticalDelta;
  const qreal second = engine.advance(QPointF(0, 160), 0.008).verticalDelta;
  QVERIFY(first > 0);
  QVERIFY(second > first);
  QVERIFY(second < engine.speedForOffset(160) * 0.008);
  const auto stopped = engine.advance(QPointF(0, 24), 0.008);
  QCOMPARE(stopped.verticalDelta, 0.0);
  QVERIFY(stopped.verticalStopped);
}

void ScrollEngineTest::reversalHasNoWrongWayCoasting() {
  ScrollEngine engine;
  for (int i = 0; i < 50; ++i)
    engine.advance(QPointF(0, 160), 0.008);
  QVERIFY(engine.advance(QPointF(0, -160), 0.008).verticalDelta < 0);
}

void ScrollEngineTest::resetClearsVelocity() {
  ScrollEngine engine;
  for (int i = 0; i < 50; ++i)
    engine.advance(QPointF(0, 160), 0.008);
  engine.stop();
  ScrollEngine fresh;
  QCOMPARE(engine.advance(QPointF(0, 160), 0.008).verticalDelta,
           fresh.advance(QPointF(0, 160), 0.008).verticalDelta);
}

void ScrollEngineTest::diagonalAxesAreIndependent() {
  ScrollEngine engine;
  const ScrollFrame frame = engine.advance(QPointF(80, -120), 0.016);
  QVERIFY(frame.horizontalActive);
  QVERIFY(frame.verticalActive);
  QVERIFY(frame.horizontalDelta > 0);
  QVERIFY(frame.verticalDelta < 0);
  QVERIFY(std::abs(frame.verticalDelta) > std::abs(frame.horizontalDelta));
}

void ScrollEngineTest::horizontalCanBeDisabled() {
  ScrollEngine engine;
  ScrollSettings settings = engine.settings();
  settings.horizontalScrolling = false;
  engine.setSettings(settings);

  const ScrollFrame frame = engine.advance(QPointF(500, 100), 0.016);
  QVERIFY(!frame.horizontalActive);
  QCOMPARE(frame.horizontalDelta, 0.0);
  QVERIFY(frame.verticalActive);
}

void ScrollEngineTest::axisStopIsEmittedOnce() {
  ScrollEngine engine;
  QVERIFY(engine.advance(QPointF(0, 100), 0.016).verticalActive);

  const ScrollFrame stopped = engine.advance(QPointF(0, 0), 0.016);
  QVERIFY(stopped.verticalStopped);
  QVERIFY(!stopped.verticalActive);

  const ScrollFrame subsequent = engine.advance(QPointF(0, 0), 0.016);
  QVERIFY(!subsequent.verticalStopped);
}

void ScrollEngineTest::directionClassification() {
  ScrollEngine engine;
  QCOMPARE(engine.directionForOffset(QPointF(0, 0)), Direction::Center);
  QCOMPARE(engine.directionForOffset(QPointF(0, -100)), Direction::North);
  QCOMPARE(engine.directionForOffset(QPointF(100, -100)), Direction::NorthEast);
  QCOMPARE(engine.directionForOffset(QPointF(100, 0)), Direction::East);
  QCOMPARE(engine.directionForOffset(QPointF(-100, 100)), Direction::SouthWest);
}

void ScrollEngineTest::lowSpeedConfigurationRemainsActive() {
  ScrollEngine engine;
  engine.setSettings({
      .deadZone = 24.0,
      .maximumSpeed = 500.0,
      .accelerationExponent = 0.8,
      .horizontalScrolling = true,
  });

  const ScrollFrame frame = engine.advance(QPointF(0, 100), 0.008);
  QVERIFY(frame.verticalActive);
  QVERIFY(frame.verticalDelta > 0.0);
  QVERIFY(frame.verticalDelta < 3.0);
}

QTEST_MAIN(ScrollEngineTest)

#include "scrollengine_test.moc"
