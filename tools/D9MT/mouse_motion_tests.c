#include "mouse_motion.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char *flags[] = {NULL, "0", "10", "1"};
    for (unsigned int index = 0; index < sizeof(flags) / sizeof(flags[0]);
         index++) {
        if (flags[index]) {
            setenv("D9MT_RELATIVE_MOUSE", flags[index], 1);
        } else {
            unsetenv("D9MT_RELATIVE_MOUSE");
        }
        assert(d9mtRelativeCameraMotion(1, 1) == (index == 3));
        assert(d9mtRelativeCameraMotion(0, 1) == 0);
        assert(d9mtRelativeCameraMotion(1, 0) == 0);
        printf("PASS relative motion flag case %u\n", index);
    }
    /* Repeated recentering must not starve already-corrected camera motion. */
    for (unsigned int index = 1; index <= 120; index++) {
        double eventTime = index / 120.0;
        double warpTime = eventTime + 0.02;
        assert(!d9mtDiscardStaleMouseMotion(1, eventTime, warpTime));
        assert(d9mtDiscardStaleMouseMotion(0, eventTime, warpTime));
    }
    puts("PASS corrected relative motion survives continuous recenters");
    assert(d9mtDiscardStaleMouseMotion(0, 1, 1));
    assert(!d9mtDiscardStaleMouseMotion(0, 2, 1));
    assert(!d9mtDiscardStaleMouseMotion(0, 0, 0));
    puts("PASS ordinary absolute motion retains warp cutoff");
    unsetenv("D9MT_RELATIVE_MOUSE");
    puts("6 passed, 0 failed");
    return 0;
}
