/* Portable regression checks for the Wine 11 window-data adapter. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef void *HWND;
typedef void *macdrv_window;
typedef void *macdrv_view;
typedef void *macdrv_metal_device;
typedef void *macdrv_metal_view;
typedef void *macdrv_metal_layer;
typedef int BOOL;
struct macdrv_win_data {
    HWND hwnd;
    macdrv_window cocoa_window;
    macdrv_view client_view;
    uintptr_t rectangles[8];
};
static struct macdrv_win_data nativeWindow;
static unsigned int lockCount;
static unsigned int releaseCount;
struct client_surface {
    int placeholder;
};
struct macdrv_client_surface {
    struct client_surface client;
};
static struct macdrv_client_surface testSurface;
static unsigned int surfaceReleases;
static struct macdrv_client_surface *macdrv_client_surface_create(HWND window) {
    assert(window == nativeWindow.hwnd);
    assert(lockCount == 0);
    nativeWindow.client_view = nativeWindow.cocoa_window;
    return &testSurface;
}
static void client_surface_release(struct client_surface *surface) {
    assert(surface == &testSurface.client);
    assert(lockCount == 0);
    ++surfaceReleases;
    nativeWindow.client_view = NULL;
}
static struct macdrv_win_data *get_win_data(HWND window) {
    if (window != nativeWindow.hwnd) {
        return NULL;
    }
    ++lockCount;
    return &nativeWindow;
}
static void release_win_data(struct macdrv_win_data *data) {
    assert(data == &nativeWindow);
    assert(lockCount == 1);
    --lockCount;
    ++releaseCount;
}
static macdrv_window macdrv_get_cocoa_window(HWND window, BOOL onScreen) {
    (void)window;
    (void)onScreen;
    return nativeWindow.cocoa_window;
}
static macdrv_metal_device macdrv_create_metal_device(void) { return NULL; }
static void macdrv_release_metal_device(macdrv_metal_device device) {
    (void)device;
}
static macdrv_metal_view
macdrv_view_create_metal_view(macdrv_view view, macdrv_metal_device device) {
    (void)device;
    assert(view == nativeWindow.client_view);
    assert(lockCount == 1);
    return view;
}
static macdrv_metal_layer macdrv_view_get_metal_layer(macdrv_metal_view view) {
    return view;
}
static void macdrv_view_release_metal_view(macdrv_metal_view view) {
    (void)view;
}
struct macdrv_thread_data {
    void *current_event;
};
static struct macdrv_thread_data testThread;
static struct macdrv_thread_data *currentThread = &testThread;
static unsigned int eventPumpCalls;
#define QS_ALLINPUT 0x1cff
static struct macdrv_thread_data *macdrv_thread_data(void) {
    return currentThread;
}
static BOOL macdrv_ProcessEvents(unsigned int mask) {
    assert(mask == QS_ALLINPUT);
    ++eventPumpCalls;
    return 1;
}
#define D9MT_BRIDGE_TEST
#include "wine_bridge.c" // NOLINT(bugprone-suspicious-include): exercise private adapter helpers

int main(void) {
    int windowToken, viewToken, cocoaToken;
    nativeWindow.hwnd = &windowToken;
    nativeWindow.client_view = &viewToken;
    nativeWindow.cocoa_window = &cocoaToken;
    nativeWindow.rectangles[0] = UINTPTR_MAX;
    struct d9mtWindowData *compatible =
        macdrv_functions.getWindowData(&windowToken);
    assert(compatible && compatible->hwnd == &windowToken);
    assert(compatible->cocoa_window == &cocoaToken);
    assert(compatible->client_cocoa_view == &viewToken);
    assert(compatible->cocoa_view == &viewToken);
    puts("PASS window-data layout");
    assert(lockCount == 1 && releaseCount == 0);
    puts("PASS native lock retained");
    macdrv_metal_view view =
        macdrv_functions.createMetalView(compatible->client_cocoa_view, NULL);
    assert(macdrv_functions.getMetalLayer(view) == &viewToken);
    puts("PASS function-table dispatch");
    macdrv_functions.releaseWindowData(compatible);
    assert(lockCount == 0 && releaseCount == 1);
    puts("PASS native lock released once");
    assert(macdrv_functions.getWindowData(NULL) == NULL);
    macdrv_functions.releaseWindowData(NULL);
    assert(lockCount == 0 && releaseCount == 1);
    puts("PASS missing-window and null-release handling");
    nativeWindow.client_view = NULL;
    compatible = macdrv_functions.getWindowData(&windowToken);
    assert(compatible && compatible->client_cocoa_view == &cocoaToken);
    puts("PASS missing client surface created");
    view =
        macdrv_functions.createMetalView(compatible->client_cocoa_view, NULL);
    macdrv_functions.releaseWindowData(compatible);
    assert(surfaceReleases == 0);
    macdrv_functions.releaseMetalView(view);
    assert(surfaceReleases == 1 && nativeWindow.client_view == NULL);
    puts("PASS client surface retained until Metal view release");
    unsetenv("D9MT_PUMP_INPUT");
    d9mtProcessPendingEvents();
    assert(eventPumpCalls == 0);
    puts("PASS event pump disabled by default");
    setenv("D9MT_PUMP_INPUT", "0", 1);
    d9mtProcessPendingEvents();
    assert(eventPumpCalls == 0);
    puts("PASS zero disables event pump");
    setenv("D9MT_PUMP_INPUT", "1", 1);
    currentThread = NULL;
    d9mtProcessPendingEvents();
    assert(eventPumpCalls == 0);
    puts("PASS missing thread skips event pump");
    currentThread = &testThread;
    testThread.current_event = &windowToken;
    d9mtProcessPendingEvents();
    assert(eventPumpCalls == 0);
    puts("PASS event handler cannot reenter pump");
    testThread.current_event = NULL;
    d9mtProcessPendingEvents();
    assert(eventPumpCalls == 1);
    puts("PASS enabled pump processes all input");
    unsetenv("D9MT_PUMP_INPUT");
    puts("12 passed, 0 failed");
    return 0;
}
