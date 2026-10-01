#!/usr/bin/env bash
# Build the Wine 11 driver adapter in a disposable source checkout.
set -euo pipefail
bridge_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$bridge_dir/../.." && pwd)"
work="${1:-$repo_root/.build/d9mt-test}"
source_dir="$work/wine-git"
revision=db11d0fe6a169c457e23d007e20404643d067aa8
mkdir -p "$work/wine-build"
if [[ ! -d "$source_dir/.git" ]]; then
    git init "$source_dir"
    git -C "$source_dir" fetch --depth 1 https://github.com/Sikarugir-App/wine.git "$revision"
    git -C "$source_dir" checkout --detach FETCH_HEAD
fi
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "$revision" ]]; then
    echo "Expected Wine 11.0 revision $revision in $source_dir" >&2
    exit 1
fi
# Use the same forward-only patch helper as the companion build.
python3 - "$bridge_dir" "$source_dir" <<'PYTHON'
from pathlib import Path
import runpy
import sys
bridge = Path(sys.argv[1])
apply_patch = runpy.run_path(str(bridge / "build-companion.py"))["apply_patch"]
for name in ("wine-driver.patch", "cursor-warp.patch", "relative-mouse.patch"):
    apply_patch(Path(sys.argv[2]), bridge / name)
PYTHON
cp "$bridge_dir/wine_bridge.c" "$source_dir/dlls/winemac.drv/d9mt_bridge.c"
cp "$bridge_dir/mouse_motion.h" "$source_dir/dlls/winemac.drv/d9mt_mouse_motion.h"
python3 - "$source_dir/dlls/winemac.drv/Makefile.in" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
source = path.read_text()
if '\td9mt_bridge.c \\' not in source:
    source = source.replace('SOURCES = \\\n', 'SOURCES = \\\n\td9mt_bridge.c \\\n')
path.write_text(source)
PY
export PATH="/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:$PATH"
cd "$work/wine-build"
"$source_dir/configure" --host=x86_64-apple-darwin --build=x86_64-apple-darwin \
    --enable-win64 --enable-archs=i386,x86_64 --disable-tests --without-x \
    --without-gnutls --without-gstreamer --without-freetype \
    CC='clang -arch x86_64' CXX='clang++ -arch x86_64' \
    BISON=/opt/homebrew/opt/bison/bin/bison FLEX=/opt/homebrew/opt/flex/bin/flex
make -j8 dlls/winemac.drv/winemac.so
nm -gU dlls/winemac.drv/winemac.so
