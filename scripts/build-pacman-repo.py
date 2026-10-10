#!/usr/bin/env python3
"""Build and optionally publish a signed pacman repository using a local key."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
KEY = "B83D9325C421436F174930DD2B63F6A3F7ECE7C8"
REPO = "sr71gatoraid/ballsack-scroll"


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--signing-home", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/pacman-repo")
    parser.add_argument("--publish", action="store_true")
    args = parser.parse_args()
    package = args.package.resolve()
    metadata = subprocess.check_output(["bsdtar", "-xOf", str(package), ".PKGINFO"], text=True)
    fields = dict(line.split(" = ", 1) for line in metadata.splitlines() if " = " in line)
    if fields.get("pkgname") != "ballsack-scroll" or fields.get("arch") != "x86_64":
        parser.error("Expected an x86_64 ballsack-scroll package")
    library = subprocess.check_output([
        "bsdtar", "-xOf", str(package), "usr/lib/qt6/plugins/kwin/effects/plugins/ballsackscroll.so"
    ])
    abi = re.search(rb"org\.kde\.kwin\.EffectPluginFactory([0-9]+\.[0-9]+\.[0-9]+)", library)
    if not abi:
        parser.error("Cannot identify the package's KWin plugin ABI")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    public_package = output / package.name
    if public_package != package:
        shutil.copy2(package, public_package)
    gpg = ["gpg", "--homedir", str(args.signing_home.resolve()), "--batch", "--yes", "--local-user", KEY]

    def sign(path):
        signature = path.with_name(path.name + ".sig")
        run(*gpg, "--output", str(signature), "--detach-sign", str(path))
        run(*gpg, "--verify", str(signature), str(path))
        return signature

    package_sig = sign(public_package)
    run("repo-add", "--include-sigs", "--nocolor", str(output / "ballsack-scroll.db.tar.gz"), str(public_package))
    database = output / "ballsack-scroll.db"
    files = output / "ballsack-scroll.files"
    database_sig = sign(database)
    files_sig = sign(files)
    public_key = output / "ballsack-scroll-key.asc"
    certificate = subprocess.check_output(gpg + ["--armor", "--export", KEY])
    if b"BEGIN PGP PUBLIC KEY BLOCK" not in certificate or b"PRIVATE" in certificate:
        raise SystemExit("Expected only a public signing certificate")
    public_key.write_bytes(certificate)
    installer = output / "setup-pacman.sh"
    shutil.copy2(ROOT / "packaging/pacman/setup-pacman.sh", installer)
    information = output / "repo-info.json"
    information.write_text(json.dumps({
        "package": fields["pkgname"], "version": fields["pkgver"], "arch": fields["arch"],
        "kwin_plugin_abi": abi.group(1).decode(), "signing_fingerprint": KEY,
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
    }, indent=2) + "\n")
    assets = [public_package, package_sig, database, database_sig, files, files_sig, public_key, installer, information]
    if args.publish:
        existing = subprocess.run(["gh", "release", "view", "pacman-repo", "--repo", REPO], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if existing.returncode:
            run("gh", "release", "create", "pacman-repo", "--repo", REPO, "--target", "main", "--prerelease",
                "--title", "Signed pacman repository", "--notes",
                "Stable repository endpoint for x86_64 Arch Linux. Package and database signatures are required. See README for setup.")
        run("gh", "release", "upload", "pacman-repo", "--repo", REPO, "--clobber", *map(str, assets))
    print(f"Signed repository: {output}")


if __name__ == "__main__":
    main()
