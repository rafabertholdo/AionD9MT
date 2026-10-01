/* Preserve event-tap relative deltas during camera dragging.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef D9MT_MOUSE_MOTION_H
#define D9MT_MOUSE_MOTION_H
#include <stdlib.h>

static inline int d9mtRelativeCameraMotion(int clipped, int rightDrag) {
    const char *enabled = getenv("D9MT_RELATIVE_MOUSE");
    return enabled && enabled[0] == '1' && enabled[1] == '\0' && clipped &&
           rightDrag;
}
/* Absolute positions before a warp are stale. Event-tap corrected relative
 * deltas describe physical motion and must survive the same cutoff. */
static inline int d9mtDiscardStaleMouseMotion(int relativeCamera,
                                              double eventTime,
                                              double lastWarpTime) {
    return !relativeCamera && lastWarpTime > 0 && eventTime <= lastWarpTime;
}
#endif
