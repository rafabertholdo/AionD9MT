"""Check delivery before Wine's cursor warp discards pending mouse events."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

BRIDGE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("builder", BRIDGE / "build-companion.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

CURSOR_SOURCE = '''/***********************************************************************
 *              SetCursorPos (MACDRV.@)
 */
BOOL macdrv_SetCursorPos(INT x, INT y)
{
    BOOL ret = macdrv_set_cursor_position(CGPointMake(x, y));
    if (ret)
        TRACE("warped to %d,%d\\n", x, y);
    else
        ERR("failed to warp to %d,%d\\n", x, y);
    return ret;
}
'''

HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
typedef int BOOL;
typedef int INT;
typedef struct { int x; int y; } CGPoint;
#define CGPointMake(x, y) ((CGPoint){x, y})
#define TRACE(...) ((void)0)
#define ERR(...) ((void)0)
#define QS_ALLINPUT 255
struct macdrv_thread_data { void *current_event; };
static struct macdrv_thread_data data;
static int pending = 3;
static int delivered;
static int warpCount;
static int mode;
static struct macdrv_thread_data *macdrv_thread_data(void) {
    return mode == 3 ? NULL : &data;
}
static void macdrv_ProcessEvents(int mask) {
    assert(mask == QS_ALLINPUT);
    assert(warpCount == 0);
    delivered += pending;
    pending = 0;
}
static BOOL macdrv_set_cursor_position(CGPoint point) {
    assert(point.x == 100 && point.y == 200);
    warpCount++;
    pending = 0;
    return mode != 4;
}
'''

MAIN = r'''
int main(int argc, char **argv) {
    assert(argc == 2);
    mode = atoi(argv[1]);
    assert(setenv("D9MT_PUMP_INPUT", mode == 1 ? "0" : "1", 1) == 0);
    data.current_event = mode == 2 ? &data : NULL;
    assert(macdrv_SetCursorPos(100, 200) == (mode != 4));
    assert(warpCount == 1);
    assert(delivered == ((mode == 0 || mode == 4) ? 3 : 0));
    assert(pending == 0);
    return 0;
}
'''


class CursorWarpTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.project = Path(self.directory.name)
        self.source = self.project / "dlls/winemac.drv/mouse.c"
        self.source.parent.mkdir(parents=True)
        self.source.write_text(CURSOR_SOURCE)
        self.patch = BRIDGE / "cursor-warp.patch"
        builder.apply_patch(self.project, self.patch)

    def test_repeated_patch_preserves_single_pump(self):
        builder.apply_patch(self.project, self.patch)
        self.assertEqual(self.source.read_text().count("d9mtProcessPendingEvents();"), 1)

    def test_queued_motion_and_guarded_warps(self):
        # Compile the patched Wine function and the real adapter guard together.
        bridge = (BRIDGE / "wine_bridge.c").read_text()
        start = bridge.index("void d9mtProcessPendingEvents(void)")
        end = bridge.index("\n}\n", start) + 3
        harness = self.project / "warp.c"
        harness.write_text(HARNESS + bridge[start:end] + self.source.read_text() + MAIN)
        executable = self.project / "warp"
        subprocess.run([
            "clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", str(harness), "-o", str(executable),
        ], check=True)
        for mode in range(5):
            with self.subTest(mode=mode):
                subprocess.run([str(executable), str(mode)], check=True)


if __name__ == "__main__":
    unittest.main()
