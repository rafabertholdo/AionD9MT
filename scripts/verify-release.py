"""Check release bytes and the runtime layout expected by ReRun."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import sys
import tarfile

root = Path(__file__).resolve().parents[1]
folder = Path(sys.argv[1])
manifest = json.loads((folder / "release-manifest.json").read_text())
archive = folder / manifest["asset"]
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
if digest != manifest["sha256"]:
    raise SystemExit("Release archive checksum mismatch")
if (folder / "SHA256SUMS").read_text() != f"{digest}  {archive.name}\n":
    raise SystemExit("SHA256SUMS does not match the release manifest")
required = {"d3d9.dll"}
required.update(f"{architecture}/{name}" for architecture in ("i386-windows", "x86_64-windows")
                for name in ("winemetal.dll", "d9mtmetal.dll"))
required.update(f"x86_64-unix/{name}" for name in ("winemetal.so", "d9mtmetal.so", "winemac.so"))
with tarfile.open(archive) as bundle:
    files = set()
    for member in bundle.getmembers():
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or not (member.isfile() or member.isdir()):
            raise SystemExit(f"Unsafe archive entry: {member.name}")
        if member.isfile():
            files.add(str(path))
    if not required.issubset(files):
        raise SystemExit(f"Missing runtime files: {sorted(required - files)}")
for name, expected in manifest["patches"].items():
    if hashlib.sha256((root / "tools/D9MT" / name).read_bytes()).hexdigest() != expected:
        raise SystemExit(f"Release source changed: {name}")
print(f"Verified {archive.name}: {digest}; all {len(required)} runtime files present")
