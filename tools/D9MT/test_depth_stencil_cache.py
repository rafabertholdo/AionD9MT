"""Check the real backend cache key across Vulkan attachment permissions."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class DepthStencilCacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        project = Path(os.environ.get("D9MT_PROJECT", ".build/d9mt-test/d9mt-main"))
        source = (project / "src/d3d9fe/d9mt_context.cpp").read_text()
        cls.fragment = source.split("static obj_handle_t getDsso(", 1)[1]
        cls.fragment = cls.fragment.split("std::lock_guard", 1)[0]
        cls.expression = re.search(
            r"\|\s*\(uint64_t\(([^\n]*readOnlyAspects[^\n]*)\) << 7\)",
            cls.fragment,
        ).group(1)

    def run_key_check(self, expression, expect_collision):
        # Execute the backend expression, then exercise actual map lookup order.
        # Stencil write permission must not depend on which attachment came first.
        harness = """
#include <cstdint>
#include <map>
#include <utility>
int main() {
  for (unsigned first = 0; first < 4; ++first) {
    std::map<uint64_t, unsigned> cache;
    for (unsigned step = 0; step < 4; ++step) {
      unsigned permission = (first + step) % 4;
      unsigned readOnlyAspects = permission * 2;
      uint64_t key = 1u | (uint64_t(EXPRESSION) << 7) | (uint64_t(255) << 29);
      unsigned writeMask = (readOnlyAspects & 4u) ? 0u : 255u;
      auto result = cache.emplace(key, writeMask);
      if (result.first->second != writeMask) return 1;
    }
    if (cache.size() != 4) return 2;
  }
  return 0;
}
""".replace("EXPRESSION", expression)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "cache.cpp"
            executable = Path(directory) / "cache"
            source.write_text(harness)
            subprocess.run(
                ["xcrun", "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 str(source), "-o", str(executable)], check=True,
            )
            result = subprocess.run([str(executable)], check=False)
            self.assertEqual(result.returncode != 0, expect_collision)

    def test_read_only_depth_and_stencil_have_distinct_cache_entries(self):
        self.run_key_check(self.expression, False)

    def test_original_key_reproduces_incorrect_stencil_write_mask(self):
        self.run_key_check("readOnlyAspects & 3u", True)


if __name__ == "__main__":
    unittest.main()
