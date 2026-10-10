#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
client_kind=${1:-native}
case "$client_kind" in native|browser) ;; *) exit 2 ;; esac
test_variant=${2:-pane}
case "$test_variant" in pane|window) ;; *) exit 2 ;; esac
runtime_build="$project_dir/build/runtime/$client_kind"
mkdir -p "$runtime_build"
wayland-scanner client-header "$project_dir/tests/runtime/fake-input.xml" "$runtime_build/fake-input-client.h"
wayland-scanner private-code "$project_dir/tests/runtime/fake-input.xml" "$runtime_build/fake-input.c"
cc -c "$runtime_build/fake-input.c" -o "$runtime_build/fake-input.o" $(pkg-config --cflags wayland-client)
c++ -std=c++20 -fPIC "$project_dir/tests/runtime/$client_kind-smoke.cpp" "$runtime_build/fake-input.o" \
  -I"$runtime_build" -o "$runtime_build/$client_kind-smoke" \
  $(pkg-config --cflags --libs Qt6Widgets Qt6DBus wayland-client)
test_config=$(mktemp -d)
trap 'rm -r -- "$test_config"' EXIT
cat > "$test_config/kwinrc" <<'CONFIG'
[Plugins]
smoothautoscrollEnabled=false
CONFIG
export XDG_CONFIG_HOME="$test_config"
export QT_PLUGIN_PATH="${SMOOTH_AUTOSCROLL_TEST_PLUGIN_ROOT:-$project_dir/build/bin}"
export QT_QPA_PLATFORM=wayland
export KWIN_COMPOSE=Q
export KWIN_WAYLAND_NO_PERMISSION_CHECKS=1
export QT_QPA_PLATFORMTHEME=generic
export QT_FORCE_STDERR_LOGGING=1
export WAYLAND_DISPLAY="smooth-autoscroll-test-$$"
export SMOOTH_AUTOSCROLL_TEST_REPORT="$runtime_build/report-$test_variant.txt"
export SMOOTH_AUTOSCROLL_TEST_VARIANT="$test_variant"
export SMOOTH_AUTOSCROLL_BROWSER_STATE="$test_config/browser-state.json"
export SMOOTH_AUTOSCROLL_ELECTRON_FIXTURE="$project_dir/tests/runtime/electron-smoke.cjs"
if [[ -f "$SMOOTH_AUTOSCROLL_TEST_REPORT" ]]; then rm -- "$SMOOTH_AUTOSCROLL_TEST_REPORT"; fi
unset DISPLAY
timeout 20s dbus-run-session -- kwin_wayland --virtual --width 800 --height 600 \
  --socket "$WAYLAND_DISPLAY" --no-lockscreen --no-global-shortcuts --no-kactivities \
  --exit-with-session "$runtime_build/$client_kind-smoke"
cat "$SMOOTH_AUTOSCROLL_TEST_REPORT"
rg -q '^Integration failures: 0$' "$SMOOTH_AUTOSCROLL_TEST_REPORT"
