# Smooth Autoscroll

Smooth, directional middle-button scrolling for **KDE Plasma on Wayland**.
Hold the middle button, move the pointer away from the four-arrow anchor, and
scroll faster as you move farther away. Release to stop.

Quick middle clicks keep their normal actions, including closing tabs and opening
links. No modifier is required. The real pointer remains visible and free;
scrolling stays at its original target even across sidebars and window edges.

The anchor is SVG artwork with charcoal arrows, a translucent grey disc, and a
subtle gleam. Its default diameter is 30 pixels on a 1440p screen and scales with
the screen's shorter side, accounting for KDE display scaling.

## Compatibility

The first public preview targets KWin **6.7.5 or newer** and is tested on Arch
Linux with KWin 6.7.5 and Qt 6.12. Native Wayland applications work best.
XWayland compatibility is best effort. GNOME, other compositors, and Plasma X11
are not supported. New KWin releases may require source changes.

Compiled KWin effects must match the installed KWin plugin ABI. **Rebuild after
KWin updates**, then log out and back in. The package deliberately does not pin
KWin to an exact version or prevent desktop updates.

## Install on Arch Linux

The package name is `kwin-smooth-autoscroll`. AUR publication is pending for the
first release; the instructions below become usable once it is published:

```sh
git clone https://aur.archlinux.org/kwin-smooth-autoscroll.git
cd kwin-smooth-autoscroll
makepkg -si
```

A release `.pkg.tar.zst` can also be installed with `sudo pacman -U`, provided its
KWin ABI matches your installed version. Matching source is included in every
release.

After installation, log out and back in. Open **System Settings → Window
Management → Desktop Effects**, enable **Smooth Autoscroll**, and use its
Configure button to adjust speed, dead zone, appearance, and excluded apps.

If your mouse has a separate “hold middle button to scroll” setting, disable it
so it does not consume the gesture or lock the pointer. This package does not
change mouse, clipboard, or primary-selection settings automatically.

## Build on other KDE distributions

Install a C++20 compiler, CMake, Ninja, Extra CMake Modules, Qt 6 development
packages (Core, DBus, Gui, Svg, Widgets, Test), KDE Frameworks 6 development
packages (Config, ConfigWidgets, CoreAddons, I18n, KCMUtils, WidgetsAddons),
KWin development headers, and Vulkan headers. Package names vary by distribution.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Then log out and back in and enable the effect in System Settings. The stable
plugin ID is `smoothautoscroll`; no session restart is attempted by the installer.

## Use

- A quick middle click goes to the application.
- Hold for 220 ms, or move beyond the dead zone while holding, to start scrolling.
- Distance controls speed; direction controls both axes smoothly.
- Return to the anchor to pause immediately.
- Release the middle button or press Escape to stop and remove the anchor.
- Add applications to the exclusion list when their own middle-button gestures
  should take precedence, including long holds and drags.

The older toggle modes are optional settings. They consume ordinary middle
clicks; the default hold mode preserves quick clicks.

## Development and releases

See [release instructions](docs/RELEASING.md) and
[validation instructions](docs/TESTING.md). Build a source archive and
checksum-verified AUR submission files with:

```sh
python scripts/make-source-release.py
```

Public project: https://github.com/sr71gatoraid/kwin-smooth-autoscroll

## Credits and license

This is a fork of [KWin AutoScroll](https://github.com/elecxyz/Kwin_AutoScroll),
starting from upstream commit `d488b82ac29cb54cffef26149727c2e15dbac6eb`.
Upstream contributors provided the original effect, settings, and lifecycle.
The fork adds native-click preservation, hold activation, smoother vector
scrolling, original-target input routing, screen-relative SVG artwork, and
release packaging. Upstream copyright notices are retained.

Licensed **GPL-2.0-or-later**; see [LICENSE](LICENSE). The new SVG artwork is
included under the same license. The original local reference PNG is not part
of the distribution.
