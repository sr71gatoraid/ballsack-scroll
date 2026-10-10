# Validation

Development validation was performed on Arch Linux, KWin 6.7.5, and Qt 6.12.
Application compatibility outside the tested versions is not implied.

Run the automated suites with `ctest --test-dir build --output-on-failure`.
They cover scroll dynamics, activation state, application exclusions, settings,
SVG rendering and transparency, screen-relative sizing, settings widgets, and
plugin metadata. GUI tests use Qt's offscreen platform.

The isolated runtime tests require an installed KWin Wayland binary, a compiler,
Qt/Wayland development files, D-Bus, and wayland-scanner. The browser variants
also require `electron` in PATH. They use temporary profiles and a private
headless compositor; they do not open accounts or inspect application content.

```sh
./scripts/nested-smoke.sh native
./scripts/nested-smoke.sh browser pane
./scripts/nested-smoke.sh browser window
```

The native test checks quick clicks, delayed and drag hold activation, free
visible pointer motion, original client scroll targeting, a screen-relative
anchor, release and hover restoration, dead-zone stops, Escape, and unloading.

The browser fixtures reproduce a large 500-pixel sideways move followed by
upward motion, first across a non-scrolling sidebar and then past a window edge.
The original vertical pane must scroll upward and stop on release.

For the Arch package, run `makepkg` with all dependencies installed, and inspect
the result with `namcap`. Verify installed plugin paths, metadata IDs, the KWin
plugin factory IID, and shared-library dependencies. A host build is not a clean
chroot build; the CI job checks declared dependencies in a fresh Arch container.
