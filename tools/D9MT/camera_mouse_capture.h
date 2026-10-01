/* Opt-in relative mouse handling while Aion drags its camera.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef D9MT_CAMERA_MOUSE_CAPTURE_H
#define D9MT_CAMERA_MOUSE_CAPTURE_H

#include <stdlib.h>

static HWND cameraCaptureWindow;
static RECT cameraCaptureBounds;
static RECT previousCursorBounds;

static inline int d9mtCameraCaptureEnabled(void) {
    const char *enabled = getenv("D9MT_CAPTURE_MOUSE");
    const char *pump = getenv("D9MT_PUMP_INPUT");
    return enabled && enabled[0] == '1' && enabled[1] == '\0' && pump &&
           pump[0] == '1' && pump[1] == '\0';
}

static inline void d9mtReleaseCameraMouseCapture(void) {
    if (cameraCaptureWindow) {
        RECT currentBounds;
        /* Do not undo a clip subsequently installed by the game or another app.
         */
        if (GetClipCursor(&currentBounds) &&
            EqualRect(&currentBounds, &cameraCaptureBounds)) {
            (void)ClipCursor(GetForegroundWindow() == cameraCaptureWindow
                                 ? &previousCursorBounds
                                 : NULL);
        }
        cameraCaptureWindow = NULL;
    }
}

static inline void d9mtUpdateCameraMouseCapture(HWND window) {
    const int wantsCapture =
        d9mtCameraCaptureEnabled() && window &&
        GetForegroundWindow() == window &&
        ((unsigned int)(unsigned short)GetAsyncKeyState(VK_RBUTTON) & 0x8000u);
    if (cameraCaptureWindow &&
        (!wantsCapture || cameraCaptureWindow != window)) {
        d9mtReleaseCameraMouseCapture();
    }
    if (wantsCapture && !cameraCaptureWindow) {
        RECT bounds;
        if (!GetClientRect(window, &bounds)) {
            return;
        }
        SetLastError(ERROR_SUCCESS);
        if ((!MapWindowPoints(window, NULL, (POINT *)&bounds, 2) &&
             GetLastError() != ERROR_SUCCESS) ||
            !GetClipCursor(&previousCursorBounds)) {
            return;
        }
        /* A desktop-sized rect would disable Wine's event-tap clipping path. */
        bounds.left++;
        bounds.top++;
        bounds.right--;
        bounds.bottom--;
        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
            return;
        }
        if (ClipCursor(&bounds)) {
            cameraCaptureBounds = bounds;
            cameraCaptureWindow = window;
        }
    }
}

#endif
