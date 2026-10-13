"""Package built components as dist/<release> and refresh release-manifest.json.

Usage, after scripts/build.sh:
    python3 scripts/package.py v0.1.2 dist/v0.1.1
The second argument is a previous release whose notices and licenses are reused.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parents[1]
release, previous = sys.argv[1], Path(sys.argv[2])
work = root / ".build/d9mt-test"
build = work / "d9mt-main/build"
out = root / "dist" / release
stage = root / ".build" / f"stage-{release}"
shutil.rmtree(stage, ignore_errors=True)
stage.mkdir(parents=True)
with tarfile.open(previous / "AionD9MT.tar.gz") as bundle:
    for name in ("NOTICE.txt", "licenses/DXMT-LICENSE", "licenses/SPIRV-Cross-LICENSE",
                 "licenses/Wine-COPYING.LIB"):
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(bundle.extractfile(name).read())
dxmt = work / "dxmt/v0.80"
sources = {
    "d3d9.dll": build / "d3d9fe.dll",
    "i386-windows/winemetal.dll": dxmt / "i386-windows/winemetal.dll",
    "x86_64-windows/winemetal.dll": dxmt / "x86_64-windows/winemetal.dll",
    "x86_64-unix/winemetal.so": dxmt / "x86_64-unix/winemetal.so",
    # The 32/64 copies carry Wine's builtin marker; the per-arch ones do not.
    "i386-windows/d9mtmetal.dll": build / "d9mtmetal/d9mtmetal32.dll",
    "x86_64-windows/d9mtmetal.dll": build / "d9mtmetal/d9mtmetal64.dll",
    "x86_64-unix/d9mtmetal.so": build / "d9mtmetal/d9mtmetal.so",
    "x86_64-unix/winemac.so": work / "wine-build/dlls/winemac.drv/winemac.so",
}
for name, source in sources.items():
    target = stage / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
out.mkdir(parents=True, exist_ok=True)
archive = out / "AionD9MT.tar.gz"
subprocess.run(["tar", "--no-xattrs", "--no-mac-metadata", "-czf", str(archive), "-C", str(stage)]
               + sorted(path.name for path in stage.iterdir()),
               check=True, env={**os.environ, "COPYFILE_DISABLE": "1"})


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


manifest = json.loads((root / "release-manifest.json").read_text())
manifest["release"] = release
manifest["sha256"] = sha256(archive)
manifest["patches"] = {path.name: sha256(path) for path in sorted((root / "tools/D9MT").iterdir())
                       if path.is_file() and path.suffix in {".patch", ".h", ".c", ".py", ".sh"}}
manifest["components"] = {str(path.relative_to(stage)): sha256(path)
                          for path in sorted(stage.rglob("*")) if path.is_file()}
text = json.dumps(manifest, indent=2) + "\n"
(root / "release-manifest.json").write_text(text)
(out / "release-manifest.json").write_text(text)
(out / "SHA256SUMS").write_text(f"{manifest['sha256']}  {archive.name}\n")
print(manifest["sha256"])
