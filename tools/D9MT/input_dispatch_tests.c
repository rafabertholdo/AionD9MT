#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static int resolveCalls;
static int pumpCalls;
static void pump(void) { pumpCalls++; }
static void *resolve(void *handle, const char *symbol) {
    (void)handle;
    assert(strcmp(symbol, "d9mtProcessPendingEvents") == 0);
    resolveCalls++;
    return (void *)pump;
}
#define dlsym resolve
#include "input_dispatch.h"
#undef dlsym

int main(void) {
    assert(d9mtDispatchDriverInput(NULL) == 0);
    assert(resolveCalls == 1 && pumpCalls == 1);
    puts("PASS native driver pump resolved and invoked");
    assert(d9mtDispatchDriverInput(NULL) == 0);
    assert(resolveCalls == 1 && pumpCalls == 2);
    puts("PASS later frames reuse resolved entry point");
    d9mtDriverInputPump = NULL;
    assert((unsigned int)d9mtDispatchDriverInput(NULL) == 0xc0000002u);
    assert(resolveCalls == 1 && pumpCalls == 2);
    puts("PASS missing driver safely reports unavailable");
    puts("3 passed, 0 failed");
    return 0;
}
