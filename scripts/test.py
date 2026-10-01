"""Run the portable renderer and patch regressions without a game client."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bridge = root / "tools/D9MT"
output = root / ".build/tests"
output.mkdir(parents=True, exist_ok=True)
for name in ("wine_bridge", "frame_input_pump", "camera_mouse_capture", "mouse_motion", "input_dispatch"):
    executable = output / name
    subprocess.run([
        "clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(bridge / f"{name}_tests.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
with tempfile.TemporaryDirectory(dir=output) as directory:
    project = Path(directory)
    target = project / "src/d3d9fe/d9mt_context.cpp"
    target.parent.mkdir(parents=True)
    shutil.copy2(root / "upstream/d9mt/src/d3d9fe/d9mt_context.cpp", target)
    subprocess.run(["patch", "--batch", "--forward", "-p1", "-i",
                    str(bridge / "depth-stencil-cache.patch")], cwd=project, check=True)
    environment = os.environ.copy()
    environment["D9MT_PROJECT"] = str(project.resolve())
    subprocess.run([
        sys.executable, "-m", "unittest", "discover", "-s", str(bridge),
        "-p", "test_*.py",
    ], env=environment, check=True)
