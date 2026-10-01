/* Opt-in Wine event pumping on the application's Present thread.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef D9MT_FRAME_INPUT_PUMP_H
#define D9MT_FRAME_INPUT_PUMP_H

#include <stdlib.h>
#ifdef _WIN32
#include "../d9mtmetal/d9mtmetal.h"
#include "camera_mouse_capture.h"
#include <windows.h>
#endif

static inline int d9mtInputPumpEnabled(void) {
    const char *enabled = getenv("D9MT_PUMP_INPUT");
    return enabled && enabled[0] == '1' && enabled[1] == '\0';
}

static inline void d9mtPumpFrameInput(void) {
    if (d9mtInputPumpEnabled()) {
        /* Reach the native queue even when Wine returns a cached cursor
         * position or its normal event-wait path does not service this driver.
         * The bridge runs on this Wine thread and retains its reentry guard.
         */
        if (D9MT_UnixCall(D9MT_FUNC_PUMP_INPUT, NULL) != 0) {
            POINT position;
            (void)GetCursorPos(&position);
        }
    }
}

static void CALLBACK d9mtPumpWindowInput(HWND window, UINT message,
                                         UINT_PTR timer, DWORD elapsed) {
    (void)window;
    (void)message;
    (void)timer;
    (void)elapsed;
#ifdef _WIN32
    d9mtUpdateCameraMouseCapture(window);
#endif
    d9mtPumpFrameInput();
}

static inline void d9mtStopInputPumpTimer(HWND *window, UINT_PTR *timer) {
#ifdef _WIN32
    d9mtReleaseCameraMouseCapture();
#endif
    if (*timer) {
        (void)KillTimer(*window, *timer);
        *timer = 0;
    }
    *window = NULL;
}

static inline void d9mtUpdateInputPumpTimer(HWND targetWindow, HWND *window,
                                            UINT_PTR *timer) {
    enum { inputPumpIntervalMilliseconds = 20 };
    if (!d9mtInputPumpEnabled() || *window != targetWindow) {
        d9mtStopInputPumpTimer(window, timer);
    }
    if (d9mtInputPumpEnabled() && targetWindow && !*timer) {
        *timer = SetTimer(targetWindow, 0, inputPumpIntervalMilliseconds,
                          d9mtPumpWindowInput);
        if (*timer) {
            *window = targetWindow;
        }
    }
#ifdef _WIN32
    if (d9mtInputPumpEnabled()) {
        d9mtUpdateCameraMouseCapture(targetWindow);
    }
#endif
}

#endif
