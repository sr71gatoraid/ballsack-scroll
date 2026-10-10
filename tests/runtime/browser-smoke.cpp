// SPDX-License-Identifier: GPL-2.0-or-later
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include "fake-input-client.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
  QCoreApplication app(argc,argv);
  if (!qEnvironmentVariable("WAYLAND_DISPLAY").startsWith("smooth-autoscroll-test-")) return 2;
  FILE *report=std::fopen(qgetenv("SMOOTH_AUTOSCROLL_TEST_REPORT").constData(),"w");
  if (!report) return 2;
  const QString statePath=qEnvironmentVariable("SMOOTH_AUTOSCROLL_BROWSER_STATE");
  QProcess browser;
  browser.setProgram("electron42");
  browser.setArguments({"--no-sandbox","--ozone-platform=wayland","--disable-gpu",
    qEnvironmentVariable("SMOOTH_AUTOSCROLL_ELECTRON_FIXTURE")});
  browser.setProcessChannelMode(QProcess::ForwardedChannels);
  browser.start();
  auto *display=wl_display_connect(nullptr);
  if (!display) return 2;
  auto *registry=wl_display_get_registry(display);
  org_kde_kwin_fake_input *fake=nullptr;
  wl_registry_listener listener={
    [](void *data,wl_registry *r,uint32_t name,const char *iface,uint32_t version) {
      if (!std::strcmp(iface,"org_kde_kwin_fake_input"))
        *static_cast<org_kde_kwin_fake_input **>(data)=
          static_cast<org_kde_kwin_fake_input *>(wl_registry_bind(r,name,
            &org_kde_kwin_fake_input_interface,std::min(version,6u)));
    }, [](void *,wl_registry *,uint32_t) {}
  };
  wl_registry_add_listener(registry,&listener,&fake);
  wl_display_roundtrip(display);
  if (!fake) return 2;
  org_kde_kwin_fake_input_authenticate(fake,"Autoscroll browser regression","Isolated test");
  wl_display_flush(display);
  auto move=[&](int x,int y) {
    org_kde_kwin_fake_input_pointer_motion_absolute(fake,wl_fixed_from_int(x),wl_fixed_from_int(y));
    wl_display_flush(display);
  };
  auto middle=[&](bool pressed) {
    org_kde_kwin_fake_input_button(fake,BTN_MIDDLE,pressed ? 1 : 0);
    wl_display_flush(display);
  };
  auto state=[&] {
    QFile file(statePath);
    if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object();
  };
  int failures=0, baseline=0;
  auto check=[&](bool ok,const char *description) {
    std::fprintf(report,"%s: %s\n",ok ? "PASS":"FAIL",description);
    std::fflush(report);
    if (!ok) ++failures;
  };
  QTimer ready;
  int attempts=0;
  QObject::connect(&ready,&QTimer::timeout,[&] {
    if (state().isEmpty()) {
      if (++attempts>70) { check(false,"Electron fixture starts"); app.exit(1); }
      return;
    }
    ready.stop();
    QDBusInterface effects("org.kde.KWin","/Effects","org.kde.kwin.Effects");
    QDBusReply<bool> loaded=effects.call("loadEffect","smoothautoscroll");
    check(loaded.isValid() && loaded.value(),"effect loads for Electron client");
    baseline=state()["y"].toInt();
    move(220,320);
    QTimer::singleShot(100,[&] { middle(true); });
    QTimer::singleShot(400,[&] { move(720,320); });
    QTimer::singleShot(900,[&] {
      const auto s=state();
      QDBusInterface effects("org.kde.KWin","/Effects","org.kde.kwin.Effects");
      check(effects.property("activeEffects").toStringList().contains("smoothautoscroll"),
            "large sideways travel keeps the hold session active");
      check(s["horizontal"].toInt()>0 && s["y"].toInt()==baseline,
            "sideways input on vertical-only content leaves it stationary");
      move(720,180);
    });
    QTimer::singleShot(1600,[&] {
      const auto s=state();
      std::fprintf(report,"After sideways-to-diagonal: y=%d, vertical events=%d\n",s["y"].toInt(),s["vertical"].toInt());
      check(s["y"].toInt()<baseline,"going far sideways into the sidebar then up scrolls the original message pane");
      baseline=s["y"].toInt();
      move(220,280);
    });
    QTimer::singleShot(2200,[&] {
      const auto s=state();
      check(s["y"].toInt()<baseline,"returning horizontally toward the pane while above the anchor continues upward scrolling");
      middle(false);
    });
    QTimer::singleShot(2550,[&] { baseline=state()["y"].toInt(); });
    QTimer::singleShot(3000,[&] {
      check(state()["y"].toInt()==baseline,"release stops Electron scrolling without momentum");
      QDBusInterface effects("org.kde.KWin","/Effects","org.kde.kwin.Effects");
      effects.call("unloadEffect","smoothautoscroll");
      std::fprintf(report,"Integration failures: %d\n",failures);
      std::fflush(report);
      app.exit(failures ? 1 : 0);
    });
  });
  ready.start(100);
  const int result=app.exec();
  browser.terminate();
  if (!browser.waitForFinished(2000)) { browser.kill(); browser.waitForFinished(2000); }
  org_kde_kwin_fake_input_destroy(fake);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  std::fclose(report);
  return result;
}
