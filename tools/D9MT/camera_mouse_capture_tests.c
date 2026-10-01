#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *HWND;
typedef struct {
    long x, y;
} POINT;
typedef struct {
    long left, top, right, bottom;
} RECT;
#define VK_RBUTTON 2
#define ERROR_SUCCESS 0
static HWND foreground;
static int rightButton;
static int clipCalls;
static int failClip;
static int failMapping;
static unsigned long lastError;
static RECT clipBounds = {0, 0, 1920, 1080};
static HWND GetForegroundWindow(void) { return foreground; }
static int GetAsyncKeyState(int key) {
    assert(key == VK_RBUTTON);
    return rightButton;
}
static int GetClientRect(HWND window, RECT *bounds) {
    assert(window == foreground);
    *bounds = (RECT){0, 0, 1920, 1080};
    return 1;
}
static void SetLastError(unsigned long error) { lastError = error; }
static unsigned long GetLastError(void) { return lastError; }
static int MapWindowPoints(HWND from, HWND to, POINT *points,
                           unsigned int count) {
    assert(from == foreground && !to && points && count == 2);
    if (failMapping) {
        lastError = 5;
    }
    return 0; /* Fullscreen at (0,0): success with no coordinate translation. */
}
static int GetClipCursor(RECT *bounds) {
    *bounds = clipBounds;
    return 1;
}
static int EqualRect(const RECT *first, const RECT *second) {
    return memcmp(first, second, sizeof(*first)) == 0;
}
static int ClipCursor(const RECT *bounds) {
    clipCalls++;
    if (failClip) {
        return 0;
    }
    clipBounds = bounds ? *bounds : (RECT){0, 0, 1920, 1080};
    return 1;
}
#include "camera_mouse_capture.h"

int main(void) {
    int windowToken, otherToken;
    foreground = &windowToken;
    rightButton = 0x8000;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 0);
    setenv("D9MT_CAPTURE_MOUSE", "10", 1);
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 0);
    puts("PASS capture requires exact opt-in");
    setenv("D9MT_CAPTURE_MOUSE", "1", 1);
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 0);
    setenv("D9MT_PUMP_INPUT", "1", 1);
    d9mtUpdateCameraMouseCapture(&otherToken);
    rightButton = 0;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 0);
    puts("PASS capture requires focused right-button drag");
    rightButton = 0x8000;
    failMapping = 1;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 0 && !cameraCaptureWindow);
    failMapping = 0;
    failClip = 1;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 1 && !cameraCaptureWindow);
    failClip = 0;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(cameraCaptureWindow == foreground && clipBounds.left == 1 &&
           clipBounds.top == 1 && clipBounds.right == 1919 &&
           clipBounds.bottom == 1079);
    puts("PASS mapping and capture failures retry, zero-offset mapping "
         "succeeds");
    d9mtUpdateCameraMouseCapture(foreground);
    assert(clipCalls == 2);
    puts("PASS held drag reuses existing capture");
    rightButton = 0;
    d9mtUpdateCameraMouseCapture(foreground);
    assert(!cameraCaptureWindow && clipBounds.left == 0 && clipCalls == 3);
    puts("PASS mouse release restores previous clip");
    rightButton = 0x8000;
    d9mtUpdateCameraMouseCapture(foreground);
    foreground = &otherToken;
    d9mtUpdateCameraMouseCapture(&windowToken);
    assert(!cameraCaptureWindow && clipBounds.left == 0 && clipCalls == 5);
    puts("PASS focus loss releases camera capture");
    foreground = &windowToken;
    d9mtUpdateCameraMouseCapture(foreground);
    clipBounds.right = 900;
    d9mtReleaseCameraMouseCapture();
    assert(clipBounds.right == 900 && clipCalls == 6);
    puts("PASS cleanup preserves a replacement clip");
    d9mtUpdateCameraMouseCapture(foreground);
    unsetenv("D9MT_CAPTURE_MOUSE");
    d9mtUpdateCameraMouseCapture(foreground);
    assert(!cameraCaptureWindow && clipBounds.right == 900 && clipCalls == 8);
    d9mtReleaseCameraMouseCapture();
    assert(clipCalls == 8);
    puts("PASS disabling and repeated cleanup restore safely");
    puts("8 passed, 0 failed");
    return 0;
}
