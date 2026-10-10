#!/usr/bin/env python3
"""Create deterministic source and AUR submission files from this checkout."""
from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import tarfile

root = Path(__file__).resolve().parents[1]
match = re.search(r"project\(kwin-smooth-autoscroll VERSION ([0-9.]+)",
                  (root / "CMakeLists.txt").read_text())
if not match:
    raise SystemExit("Cannot read release version")
version = match.group(1)
name = f"kwin-smooth-autoscroll-{version}"
epoch = int(os.environ.get("SOURCE_DATE_EPOCH") or subprocess.check_output(
    ["git", "log", "-1", "--format=%ct"], cwd=root, text=True).strip())
dist = root / "dist"
aur = dist / "aur"
aur.mkdir(parents=True, exist_ok=True)
archive = dist / f"{name}.tar.xz"
files = subprocess.check_output(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=root
).decode().split("\0")
with tarfile.open(archive, "w:xz", format=tarfile.GNU_FORMAT) as output:
    directory = tarfile.TarInfo(name)
    directory.type = tarfile.DIRTYPE
    directory.mode = 0o755
    directory.mtime = epoch
    output.addfile(directory)
    for relative in sorted(set(filter(None, files))):
        path = root / relative
        if not path.is_file():
            continue
        if path.is_symlink():
            raise SystemExit(f"Unexpected source symlink: {relative}")
        info = output.gettarinfo(str(path), arcname=f"{name}/{relative}")
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = epoch
        info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
        with path.open("rb") as source:
            output.addfile(info, source)
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
recipe = (root / "packaging/aur/PKGBUILD.in").read_text()
(aur / "PKGBUILD").write_text(recipe.replace("@VERSION@", version).replace("@SHA256@", checksum))
shutil.copy2(root / "packaging/aur/kwin-smooth-autoscroll.install", aur)
with (aur / ".SRCINFO").open("w") as destination:
    subprocess.run(["makepkg", "--printsrcinfo"], cwd=aur, stdout=destination, check=True)
(dist / "SHA256SUMS").write_text(f"{checksum}  {archive.name}\n")
print(f"Source: {archive}\nAUR files: {aur}\nSHA256: {checksum}")
