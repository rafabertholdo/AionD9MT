"""Build the d9mt companion with the isolated Wine 11 driver loader."""
import argparse
import os
from pathlib import Path
import subprocess
import shutil


def apply_patch(project, patch):
    """Apply once; forbid patch's automatic reversal of existing fixes."""
    command = ["patch", "--batch", "--forward", "-p1", "-i", str(patch)]
    check = subprocess.run(command + ["--dry-run"], cwd=project, capture_output=True)
    if check.returncode == 0:
        subprocess.run(command, cwd=project, check=True)
    else:
        # Fuzz: a later patch may have inserted lines inside this patch's context.
        subprocess.run(
            ["patch", "--batch", "--dry-run", "--reverse", "--fuzz=3", "-p1", "-i", str(patch)],
            cwd=project, check=True,
        )


def copy_frame_input_headers(project, bridge):
    """Invalidate the frontend object when its input helpers change."""
    changed = False
    for name, destination in (
        ("frame_input_pump.h", "d9mt_frame_input_pump.h"),
        ("camera_mouse_capture.h", "camera_mouse_capture.h"),
    ):
        source = bridge / name
        target = project / "src/d3d9fe" / destination
        if not target.exists() or target.read_bytes() != source.read_bytes():
            shutil.copy2(source, target)
            changed = True
    if changed:
        for cached in (project / "build").glob(
            "dxvkfe-obj*/vendor_dxvk_src_d3d9_d3d9_swapchain_cpp.o"
        ):
            cached.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="Downloaded neo773/d9mt source directory")
    args = parser.parse_args()
    project = args.project.resolve()
    bridge = Path(__file__).resolve().parent
    for name in ("d9mt.patch", "compute-dispatch.patch", "depth-stencil-cache.patch", "metal-invariance.patch", "skip-shaders.patch", "frame-input.patch", "timer-input.patch", "direct-input.patch", "data-buffer-protection.patch"):
        apply_patch(project, bridge / name)
    copy_frame_input_headers(project, bridge)
    shutil.copy2(bridge / "input_dispatch.h", project / "src/d9mtmetal/d9mt_input_dispatch.h")
    shutil.copy2(bridge / "data_buffer_protection.h", project / "src/d9mtmetal/data_buffer_protection.h")
    source = (project / "tools/build-d9mtmetal.sh").read_text()
    source = source.split('echo "[d9mtmetal] installing into CrossOver')[0]
    source = source.replace('"$SRC/unix.m" \\', '"$SRC/unix.m" \\\n  "$D9MT_BRIDGE_LOADER" \\')
    script = project / "tools/build-d9mtmetal-local.sh"
    script.write_text(source)
    build_environment = os.environ.copy()
    build_environment["D9MT_BRIDGE_LOADER"] = str(bridge / "mac_driver_loader.c")
    subprocess.run(["bash", str(script)], env=build_environment, check=True)


if __name__ == "__main__":
    main()
