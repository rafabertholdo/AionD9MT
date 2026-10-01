"""Exercise the real patch tool, including macOS automatic reversal."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "build_companion", Path(__file__).with_name("build-companion.py")
)
build_companion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_companion)


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.project = Path(self.directory.name)
        self.source = self.project / "source.txt"
        self.source.write_text("old\n")
        self.patch = self.project / "fix.patch"
        self.patch.write_text("--- a/source.txt\n+++ b/source.txt\n@@ -1 +1 @@\n-old\n+fixed\n")

    def test_applies_fresh_patch(self):
        build_companion.apply_patch(self.project, self.patch)
        self.assertEqual(self.source.read_text(), "fixed\n")

    def test_repeated_application_keeps_fix(self):
        for _ in range(3):
            build_companion.apply_patch(self.project, self.patch)
            self.assertEqual(self.source.read_text(), "fixed\n")

    def test_accepts_fix_whose_context_a_later_patch_changed(self):
        self.source.write_text("a\nb\nc\nold\nd\ne\nf\n")
        self.patch.write_text("--- a/source.txt\n+++ b/source.txt\n@@ -1,7 +1,7 @@\n a\n b\n c\n-old\n+fixed\n d\n e\n f\n")
        later = self.project / "later.patch"
        later.write_text("--- a/source.txt\n+++ b/source.txt\n@@ -1,7 +1,8 @@\n a\n b\n c\n+later\n fixed\n d\n e\n f\n")
        build_companion.apply_patch(self.project, self.patch)
        build_companion.apply_patch(self.project, later)
        build_companion.apply_patch(self.project, self.patch)
        self.assertEqual(self.source.read_text(), "a\nb\nc\nlater\nfixed\nd\ne\nf\n")

    def test_rejects_conflicting_source(self):
        self.source.write_text("conflict\n")
        with self.assertRaises(subprocess.CalledProcessError):
            build_companion.apply_patch(self.project, self.patch)
        self.assertEqual(self.source.read_text(), "conflict\n")

    def test_input_header_change_invalidates_all_frontend_configurations(self):
        bridge = self.project / "bridge"
        bridge.mkdir()
        (self.project / "src/d3d9fe").mkdir(parents=True)
        for name in ("frame_input_pump.h", "camera_mouse_capture.h"):
            (bridge / name).write_text(name)
        cached_objects = []
        for configuration in ("dxvkfe-obj", "dxvkfe-obj-release", "dxvkfe-obj-tracy-hud"):
            folder = self.project / "build" / configuration
            folder.mkdir(parents=True)
            cached = folder / "vendor_dxvk_src_d3d9_d3d9_swapchain_cpp.o"
            cached.write_text("cached")
            cached_objects.append(cached)
            (folder / "unrelated.o").write_text("keep")
        build_companion.copy_frame_input_headers(self.project, bridge)
        self.assertTrue(all(not cached.exists() for cached in cached_objects))
        self.assertTrue(all((cached.parent / "unrelated.o").exists() for cached in cached_objects))
        self.assertEqual((self.project / "src/d3d9fe/camera_mouse_capture.h").read_text(), "camera_mouse_capture.h")

    def test_unchanged_input_headers_keep_frontend_cache(self):
        bridge = self.project / "bridge"
        bridge.mkdir()
        (self.project / "src/d3d9fe").mkdir(parents=True)
        for name in ("frame_input_pump.h", "camera_mouse_capture.h"):
            (bridge / name).write_text(name)
        build_companion.copy_frame_input_headers(self.project, bridge)
        cached = self.project / "build/dxvkfe-obj/vendor_dxvk_src_d3d9_d3d9_swapchain_cpp.o"
        cached.parent.mkdir(parents=True)
        cached.write_text("cached")
        build_companion.copy_frame_input_headers(self.project, bridge)
        self.assertEqual(cached.read_text(), "cached")


if __name__ == "__main__":
    unittest.main()
