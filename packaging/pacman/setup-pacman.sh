#!/usr/bin/env bash
# Add the public signed repository, then install with a full system upgrade.
set -euo pipefail
base=https://github.com/sr71gatoraid/ballsack-scroll/releases/download/pacman-repo
fingerprint=B83D9325C421436F174930DD2B63F6A3F7ECE7C8
case "${1:-}" in
  '') install_package=yes ;;
  --configure-only) install_package=no ;;
  *) printf 'Usage: %s [--configure-only]\n' "$0" >&2; exit 2 ;;
esac
[[ $(uname -m) == x86_64 ]] || { echo 'This repository supports x86_64.' >&2; exit 1; }
command -v pacman >/dev/null
command -v gpg >/dev/null
command -v curl >/dev/null
as_root() {
  if (( EUID == 0 )); then "$@"; else sudo "$@"; fi
}
temporary=$(mktemp -d)
trap 'rm -rf -- "$temporary"' EXIT
mkdir -m700 "$temporary/gnupg"
curl -fsSL "$base/ballsack-scroll-key.asc" -o "$temporary/key.asc"
actual=$(gpg --no-options --homedir "$temporary/gnupg" --batch --with-colons --show-keys "$temporary/key.asc" | awk -F: '$1 == "fpr" { print $10; exit }')
[[ $actual == "$fingerprint" ]] || { echo 'Repository key fingerprint mismatch.' >&2; exit 1; }
as_root pacman-key --init
as_root pacman-key --add "$temporary/key.asc"
as_root pacman-key --lsign-key "$fingerprint"
cat > "$temporary/repo.conf" <<EOF
[ballsack-scroll]
SigLevel = Required
Server = $base
EOF
as_root install -Dm644 "$temporary/repo.conf" /etc/pacman.d/ballsack-scroll-repo.conf
include='Include = /etc/pacman.d/ballsack-scroll-repo.conf'
if ! grep -Fxq "$include" /etc/pacman.conf; then
  printf '\n%s\n' "$include" | as_root tee -a /etc/pacman.conf >/dev/null
fi
echo 'Signed ballsack-scroll repository configured.'
if [[ $install_package == yes ]]; then
  as_root pacman -Syu ballsack-scroll
  echo 'Log out and back in, then enable Ballsack Scroll in Desktop Effects.'
fi
