#!/usr/bin/env bash
# Build in an isolated workspace; do not install into any app or prefix.
set -euo pipefail
renderer_root="$(cd "$(dirname "$0")/.." && pwd)"
work="$renderer_root/.build/d9mt-test"
mkdir -p "$work"
if [[ ! -d "$work/d9mt-main" ]]; then
    cp -R "$renderer_root/upstream/d9mt" "$work/d9mt-main"
fi
python3 - "$renderer_root" "$work" <<'PY'
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tarfile
root, work = map(Path, sys.argv[1:])
pin = json.loads((root / "release-manifest.json").read_text())["dxmt"]
archive = work / pin["asset"]
if not archive.exists():
    subprocess.run(["curl", "-fL", "--retry", "3", "-o", str(archive),
                    f'{pin["repository"]}/releases/download/{pin["release"]}/{pin["asset"]}'], check=True)
if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
    archive.unlink()
    raise SystemExit("DXMT download checksum mismatch; retry the build")
with tarfile.open(archive) as bundle:
    bundle.extractall(work / "dxmt", filter="data")
# Use upstream's import-library generator, replacing its network fetch with the
# pinned, verified input above. Run only the generator, never its install commands.
project = work / "d9mt-main"
prebuilt = project / "prebuilt"
prebuilt.mkdir(exist_ok=True)
import shutil
for architecture, name in (("i386-windows", "winemetal32.dll"),
                           ("x86_64-windows", "winemetal.dll"),
                           ("x86_64-unix", "winemetal.so")):
    source = "winemetal.so" if architecture.endswith("unix") else "winemetal.dll"
    shutil.copy2(work / "dxmt" / pin["release"] / architecture / source, prebuilt / name)
generator = (project / "scripts/fetch-winemetal.sh").read_text().split(
    'echo "[fetch-winemetal] Generating 32-bit import library', 1)[1]
generator = 'echo "[fetch-winemetal] Generating 32-bit import library' + generator
subprocess.run(["bash", "-euc", 'PREBUILT_DIR="$1"\n' + generator, "generator", str(prebuilt)], check=True)
PY
bash "$renderer_root/tools/D9MT/build-driver.sh" "$work"
python3 "$renderer_root/tools/D9MT/build-companion.py" "$work/d9mt-main"
# Upstream's cache does not track all included headers.
touch "$work/d9mt-main"/vendor/dxvk/src/d3d9/*.cpp
bash "$work/d9mt-main/scripts/build-dxvkfe.sh"
