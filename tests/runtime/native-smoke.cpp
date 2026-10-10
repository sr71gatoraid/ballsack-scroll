// SPDX-License-Identifier: GPL-2.0-or-later
// Runs only inside the private compositor launched by nested-smoke.sh.
#include <QApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include "fake-input-client.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

class Receiver : public QWidget {
public:
  int wheels = 0, horizontal = 0, vertical = 0, clicks = 0, releases = 0, moves = 0;
  QPointF pointer;
  Receiver() { setMouseTracking(true); }
  void wheelEvent(QWheelEvent *e) override {
    ++wheels;
    horizontal += std::abs(e->pixelDelta().x());
    vertical += std::abs(e->pixelDelta().y());
    e->accept();
  }
  void mousePressEvent(QMouseEvent *) override { ++clicks; }
  void mouseReleaseEvent(QMouseEvent *) override { ++releases; }
  void mouseMoveEvent(QMouseEvent *e) override {
    ++moves;
    pointer = e->globalPosition();
  }
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  FILE *report = std::fopen(qgetenv("SMOOTH_AUTOSCROLL_TEST_REPORT").constData(), "w");
  if (!report) return 2;
  if (!qEnvironmentVariable("WAYLAND_DISPLAY").startsWith("smooth-autoscroll-test-")) {
    qCritical() << "Refusing to inject input outside the isolated test compositor";
    return 2;
  }
  Receiver window;
  window.showFullScreen();
  auto *display = wl_display_connect(nullptr);
  if (!display) return 2;
  auto *registry = wl_display_get_registry(display);
  org_kde_kwin_fake_input *fake = nullptr;
  wl_registry_listener listener = {
    [](void *data, wl_registry *r, uint32_t name, const char *iface, uint32_t version) {
      if (!std::strcmp(iface, "org_kde_kwin_fake_input"))
        *static_cast<org_kde_kwin_fake_input **>(data) =
          static_cast<org_kde_kwin_fake_input *>(wl_registry_bind(r, name,
            &org_kde_kwin_fake_input_interface, std::min(version, 6u)));
    },
    [](void *, wl_registry *, uint32_t) {}
  };
  wl_registry_add_listener(registry, &listener, &fake);
  wl_display_roundtrip(display);
  if (!fake) { qCritical() << "Isolated compositor did not expose fake input"; return 2; }
  org_kde_kwin_fake_input_authenticate(fake, "Smooth Autoscroll smoke test", "Isolated integration test");
  wl_display_flush(display);
  auto move = [&](int x, int y) {
    org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_int(x), wl_fixed_from_int(y));
    wl_display_flush(display);
  };
  auto click = [&] {
    org_kde_kwin_fake_input_button(fake, BTN_MIDDLE, 1);
    org_kde_kwin_fake_input_button(fake, BTN_MIDDLE, 0);
    wl_display_flush(display);
  };
  auto middle = [&](bool pressed) {
    org_kde_kwin_fake_input_button(fake, BTN_MIDDLE, pressed ? 1 : 0);
    wl_display_flush(display);
  };
  auto active = [&] {
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    return effects.property("activeEffects").toStringList().contains("smoothautoscroll");
  };
  auto pointerState = [&] {
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    QDBusReply<QString> reply = effects.call("debug", "smoothautoscroll", "");
    return QJsonDocument::fromJson(reply.value().toUtf8()).object();
  };
  int failures = 0, baseline = 0, movingCount = 0;
  auto check = [&](bool ok, const char *description) {
    std::fprintf(report, "%s: %s\n", ok ? "PASS" : "FAIL", description);
    std::fflush(report);
    if (ok) qInfo() << "PASS:" << description;
    else { qCritical() << "FAIL:" << description; ++failures; }
  };
  QTimer::singleShot(500, [&] {
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    QDBusReply<bool> loaded = effects.call("loadEffect", "smoothautoscroll");
    check(loaded.isValid() && loaded.value(), "effect loads into matching KWin");
    move(400, 300);
  });
  QTimer::singleShot(650, [&] { click(); });
  QTimer::singleShot(750, [&] {
    check(window.clicks == 1 && window.releases == 1, "ordinary middle click reaches native handler as a complete pair");
    check(!active() && window.wheels == 0, "ordinary click does not open the scroller");
    window.clicks = window.releases = 0;
    middle(true);
  });
  QTimer::singleShot(1050, [&] {
    check(active(), "holding without movement opens the anchor after the hold threshold");
    check(pointerState()["glyphSize"].toDouble() == 12.5,
          "anchor scales to the 600-pixel short side of the test screen");
    move(400, 430);
  });
  QTimer::singleShot(1250, [&] {
    check(window.wheels > 0 && window.vertical > 0, "native Wayland client receives continuous scroll");
    check(window.clicks == 0, "activation click is suppressed");
    const auto pointer = pointerState();
    check(pointer["pointerY"].toDouble() > 400 &&
          pointer["visiblePointerY"].toDouble() > 400,
          "real visible pointer remains free to move");
    check(pointer["clientPointerY"].toDouble() == pointer["anchorY"].toDouble(),
          "client scroll position stays at the anchor while the visible pointer moves");
    movingCount = window.wheels;
  });
  QTimer::singleShot(1500, [&] {
    check(window.wheels > movingCount, "scroll continues while mouse is stationary");
    move(500, 430);
  });
  QTimer::singleShot(1800, [&] {
    check(window.horizontal > 0, "diagonal motion scrolls horizontally too");
    move(400, 300);
  });
  QTimer::singleShot(1900, [&] { baseline = window.wheels; });
  QTimer::singleShot(2100, [&] {
    check(window.wheels == baseline, "returning to anchor stops without coasting");
    move(400, 440);
  });
  QTimer::singleShot(2350, [&] {
    middle(false);
  });
  QTimer::singleShot(2450, [&] { baseline = window.wheels; });
  QTimer::singleShot(2650, [&] {
    check(window.wheels == baseline && !active(), "hold release stops scrolling and removes the anchor");
    check(window.clicks == 0 && window.releases == 0, "scrolling hold does not leak a native middle click");
    check(window.pointer.y() == 440, "release restores client hover at the real pointer position");
    click();
  });
  QTimer::singleShot(2800, [&] {
    check(window.clicks == 1 && window.releases == 1 && !active(), "native clicks work again immediately after a scrolling hold");
    middle(true);
    move(400, 300);
  });
  QTimer::singleShot(2950, [&] {
    check(active(), "moving past the dead zone starts a hold without waiting for the timer");
    movingCount = window.wheels;
  });
  QTimer::singleShot(3150, [&] {
    check(window.wheels > movingCount, "immediate drag produces scrolling");
    org_kde_kwin_fake_input_keyboard_keysym(fake, 0xff1b, 1);
    org_kde_kwin_fake_input_keyboard_keysym(fake, 0xff1b, 0);
    wl_display_flush(display);
  });
  QTimer::singleShot(3250, [&] { baseline = window.wheels; middle(false); });
  QTimer::singleShot(3450, [&] {
    std::fprintf(report, "Wheel count after Escape: %d -> %d\n", baseline, window.wheels);
    check(window.wheels == baseline, "Escape cancels the scrolling session");
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    effects.call("unloadEffect", "smoothautoscroll");
    QDBusReply<bool> loaded = effects.call("isEffectLoaded", "smoothautoscroll");
    check(loaded.isValid() && !loaded.value(), "effect unloads cleanly");
    qInfo() << "Integration failures:" << failures;
    std::fprintf(report, "Integration failures: %d\n", failures);
    std::fflush(report);
    app.exit(failures ? 1 : 0);
  });
  const int result = app.exec();
  org_kde_kwin_fake_input_destroy(fake);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  std::fclose(report);
  return result;
}
