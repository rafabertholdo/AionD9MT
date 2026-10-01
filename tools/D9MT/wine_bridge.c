/* Wine 11 adapter for DXMT v0.80's private Mac-driver interface.
 * Compiled as part of winemac.so. SPDX-License-Identifier: LGPL-2.1-or-later
 */
#if 0
#pragma makedep unix
#endif
#ifndef D9MT_BRIDGE_TEST
#include "config.h"
#include "macdrv.h"
#endif
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>

/* Aion polls cursor position continuously with this D3D9 backend. Its native
 * events otherwise remain queued in this mixed Wine runtime. Keep the
 * compatibility pump opt-in and avoid reentry from an event handler.
 */
__attribute__((visibility("default"))) void d9mtProcessPendingEvents(void) {
    const char *enabled = getenv("D9MT_PUMP_INPUT");
    if (!enabled || enabled[0] != '1' || enabled[1] != '\0') {
        return;
    }
    struct macdrv_thread_data *data = macdrv_thread_data();
    if (data && !data->current_event) {
        macdrv_ProcessEvents(QS_ALLINPUT);
    }
}

/* Preserve the four-pointer prefix read by winemetal v0.80. The native
 * Wine 11 structure has client_view at offset 2 and rectangles at offset 3.
 * Hold the native window-data lock until winemetal releases this snapshot.
 */
struct d9mtWindowData {
    HWND hwnd;
    macdrv_window cocoa_window;
    macdrv_view cocoa_view;
    macdrv_view client_cocoa_view;
    struct macdrv_win_data *nativeData;
    struct macdrv_client_surface *createdSurface;
};

struct d9mtMetalViewRecord {
    macdrv_metal_view view;
    struct macdrv_client_surface *surface;
    struct d9mtMetalViewRecord *next;
};
static _Thread_local struct d9mtWindowData *pendingWindow;
static struct d9mtMetalViewRecord *metalViews;
static pthread_mutex_t metalViewsMutex = PTHREAD_MUTEX_INITIALIZER;

static struct d9mtWindowData *getCompatibleWindowData(HWND window) {
    struct d9mtWindowData *compatible = calloc(1, sizeof(*compatible));
    if (!compatible) {
        return NULL;
    }
    compatible->nativeData = get_win_data(window);
    if (!compatible->nativeData) {
        free(compatible);
        return NULL;
    }
    if (!compatible->nativeData->client_view) {
        release_win_data(compatible->nativeData);
        compatible->createdSurface = macdrv_client_surface_create(window);
        compatible->nativeData = get_win_data(window);
        if (!compatible->createdSurface || !compatible->nativeData) {
            if (compatible->nativeData) {
                release_win_data(compatible->nativeData);
            }
            if (compatible->createdSurface) {
                client_surface_release(&compatible->createdSurface->client);
            }
            free(compatible);
            return NULL;
        }
    }
    compatible->hwnd = compatible->nativeData->hwnd;
    compatible->cocoa_window = compatible->nativeData->cocoa_window;
    compatible->cocoa_view = compatible->nativeData->client_view;
    compatible->client_cocoa_view = compatible->nativeData->client_view;
    pendingWindow = compatible;
    return compatible;
}

static void releaseCompatibleWindowData(struct d9mtWindowData *compatible) {
    if (!compatible) {
        return;
    }
    release_win_data(compatible->nativeData);
    if (pendingWindow == compatible) {
        pendingWindow = NULL;
    }
    if (compatible->createdSurface) {
        client_surface_release(&compatible->createdSurface->client);
    }
    free(compatible);
}

static macdrv_metal_view createCompatibleMetalView(macdrv_view view,
                                                   macdrv_metal_device device) {
    struct d9mtMetalViewRecord *record = NULL;
    if (pendingWindow && pendingWindow->createdSurface) {
        record = calloc(1, sizeof(*record));
        if (!record) {
            return NULL;
        }
    }
    macdrv_metal_view metalView = macdrv_view_create_metal_view(view, device);
    if (!metalView) {
        free(record);
        return NULL;
    }
    if (record) {
        record->view = metalView;
        record->surface = pendingWindow->createdSurface;
        pendingWindow->createdSurface = NULL;
        pthread_mutex_lock(&metalViewsMutex);
        record->next = metalViews;
        metalViews = record;
        pthread_mutex_unlock(&metalViewsMutex);
    }
    return metalView;
}

static void releaseCompatibleMetalView(macdrv_metal_view view) {
    struct d9mtMetalViewRecord *record = NULL;
    pthread_mutex_lock(&metalViewsMutex);
    struct d9mtMetalViewRecord **entry = &metalViews;
    while (*entry) {
        if ((*entry)->view == view) {
            record = *entry;
            *entry = record->next;
            break;
        }
        entry = &(*entry)->next;
    }
    pthread_mutex_unlock(&metalViewsMutex);
    macdrv_view_release_metal_view(view);
    if (record) {
        client_surface_release(&record->surface->client);
        free(record);
    }
}

/* Function order is the ABI defined by winemetal v0.80. Reserved slots
 * are not used by its CreateMetalViewFromHWND/ReleaseMetalView paths.
 */
struct d9mtMacDriverFunctions {
    void (*reservedDisplayInitialization)(BOOL);
    struct d9mtWindowData *(*getWindowData)(HWND);
    void (*releaseWindowData)(struct d9mtWindowData *);
    macdrv_window (*getCocoaWindow)(HWND, BOOL);
    macdrv_metal_device (*createMetalDevice)(void);
    void (*releaseMetalDevice)(macdrv_metal_device);
    macdrv_metal_view (*createMetalView)(macdrv_view, macdrv_metal_device);
    macdrv_metal_layer (*getMetalLayer)(macdrv_metal_view);
    void (*releaseMetalView)(macdrv_metal_view);
    void (*reservedMainThreadDispatch)(void *);
};

_Static_assert(offsetof(struct d9mtWindowData, client_cocoa_view) ==
                   3 * sizeof(void *),
               "winemetal window-data ABI changed");
_Static_assert(offsetof(struct d9mtMacDriverFunctions, createMetalView) ==
                   6 * sizeof(void *),
               "winemetal function-table ABI changed");

__attribute__((visibility("default")))
const struct d9mtMacDriverFunctions macdrv_functions = {
    NULL,
    getCompatibleWindowData,
    releaseCompatibleWindowData,
    macdrv_get_cocoa_window,
    macdrv_create_metal_device,
    macdrv_release_metal_device,
    createCompatibleMetalView,
    macdrv_view_get_metal_layer,
    releaseCompatibleMetalView,
    NULL};
