/* Service the Wine thread's native queue through the companion Unix bridge.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef D9MT_INPUT_DISPATCH_H
#define D9MT_INPUT_DISPATCH_H
#include <dlfcn.h>
#include <pthread.h>

static void (*d9mtDriverInputPump)(void);
static pthread_once_t d9mtInputResolver = PTHREAD_ONCE_INIT;

static void d9mtResolveDriverInput(void) {
    d9mtDriverInputPump =
        (void (*)(void))dlsym(RTLD_DEFAULT, "d9mtProcessPendingEvents");
}

static int d9mtDispatchDriverInput(void *arguments) {
    (void)arguments;
    pthread_once(&d9mtInputResolver, d9mtResolveDriverInput);
    if (!d9mtDriverInputPump) {
        return (int)0xc0000002u;
    }
    d9mtDriverInputPump();
    return 0;
}
#endif
