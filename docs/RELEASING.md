# Release 0.1.0

The first public release is a preview for KDE Plasma on Wayland. The GitHub
project is `sr71gatoraid/kwin-smooth-autoscroll`; the AUR package name is
`kwin-smooth-autoscroll`.

1. Run the build, automated suites, and isolated runtime tests in TESTING.md.
2. Commit the release source. Set `SOURCE_DATE_EPOCH` to that commit's timestamp
   when regenerating artifacts to obtain the same source archive.
3. Run `python scripts/make-source-release.py`. It produces the source tarball,
   `SHA256SUMS`, and `dist/aur/{PKGBUILD,.SRCINFO,kwin-smooth-autoscroll.install}`.
4. Build the package without installing it:

   ```sh
   export SRCDEST="$PWD/dist"
   export PKGDEST="$PWD/dist"
   export BUILDDIR="$PWD/build-aur"
   cd dist/aur
   makepkg --cleanbuild --noconfirm
   ```

5. Inspect it with namcap, confirm the effect and configuration plugin IDs, and
   verify the embedded KWin factory IID matches the build headers. The binary
   package is tied to the build KWin ABI; the AUR recipe builds locally instead.
6. Authenticate with `gh auth login --web --hostname github.com` and create the
   public GitHub repository if it does not exist. Push the source and create
   tag `v0.1.0`. Attach the source archive, `SHA256SUMS`, binary package, and build
   metadata to the GitHub release. Mark the first preview as a prerelease.
7. Only after the source URL downloads and passes its checksum, push the three
   AUR files to `ssh://aur@aur.archlinux.org/kwin-smooth-autoscroll.git` using the
   maintainer's registered AUR SSH key. AUR stores recipes and metadata, not
   binaries. Its first package commit requires a registered AUR account.
8. Once published, update the README's pending-publication note.

The install script prints setup guidance. Packaging does not enable the effect
for all users, modify input devices, alter clipboard behavior, or pin KWin to a
particular desktop version. Rebuild after KWin upgrades.

The fork retains the upstream Git history and GPL license. The historical
cross-distribution binary build matrix is omitted because this fork has not
validated those older KWin versions. Other KDE distributions can build the
public source against their own development headers.
