# Release and signed repository

1. Bump the source version and changelog. Run the suites and isolated runtime
   tests in TESTING.md, then commit the release source.
2. Run `python scripts/make-source-release.py`. Its source archive uses the
   latest commit timestamp, or explicit `SOURCE_DATE_EPOCH`, for reproducibility.
   The script writes the archive, SHA256SUMS, and `dist/arch` package recipe.
3. Build with `makepkg --cleanbuild --noconfirm` as an ordinary user in a clean
   current Arch environment. Set SRCDEST and PKGDEST to the absolute dist path.
   Run namcap and test the packaged plugin in a nested compositor.
4. Push source and an annotated version tag. Publish the corresponding source,
   binary package, checksums, and build metadata as a GitHub version release.
   GPL corresponding source must remain publicly available with its binary.
5. Build and publish the signed repository using the dedicated local key:

   ```sh
   python scripts/build-pacman-repo.py \
     --package dist/ballsack-scroll-VERSION-REL-x86_64.pkg.tar.zst \
     --signing-home ~/.local/share/ballsack-scroll/repository-signing \
     --publish
   ```

The signing key's private material stays outside the checkout and is never
uploaded to GitHub. Only its public certificate, package and database
signatures, repository databases, installer, and build information are uploaded
to the stable `pacman-repo` release. Keep a secure backup of the private key.
The certificate expires in October 2028; renew it and publish its public
certificate before expiration. A replacement key requires updating the setup
fingerprint and having existing users explicitly trust the replacement.

Package updates use normal pacman version ordering. Increment pkgrel for a
rebuild against a newer KWin ABI, even when the source version is unchanged.
The package does not pin KWin or block desktop upgrades. Verify every published
update by installing from the remote signed repository in a clean Arch container.

The installer configures pacman; it does not enable the effect for every user
or alter mouse and clipboard settings. Enable Ballsack Scroll in Desktop Effects.
The stable plugin ID is ballsackscroll; the existing Effect-smoothautoscroll
settings group is retained to preserve preferences. Log out after upgrades.

The fork retains upstream Git history and GPL notices. Other KDE distributions
can build the public source against their own development headers.
