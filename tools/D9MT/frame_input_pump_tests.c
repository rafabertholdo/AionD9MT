#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    long x;
    long y;
} POINT;

#define CALLBACK
typedef void *HWND;
typedef unsigned int UINT;
typedef unsigned long UINT_PTR;
typedef unsigned long DWORD;
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

static int timerCalls;
static int killCalls;
static UINT_PTR timerResult = 1;
static UINT timerInterval;
static TIMERPROC timerCallback;

static UINT_PTR SetTimer(HWND window, UINT_PTR timer, UINT interval,
                         TIMERPROC callback) {
    (void)window;
    (void)timer;
    timerCalls++;
    timerInterval = interval;
    timerCallback = callback;
    return timerResult;
}

static int KillTimer(HWND window, UINT_PTR timer) {
    (void)window;
    (void)timer;
    killCalls++;
    return 1;
}

#define D9MT_FUNC_PUMP_INPUT 5u
static int unixCalls;
static int unixResult;
static int pendingMovement;
static int deliveredMovement;
static int D9MT_UnixCall(unsigned int code, void *params) {
    assert(code == D9MT_FUNC_PUMP_INPUT && params == NULL);
    unixCalls++;
    if (!unixResult) {
        deliveredMovement += pendingMovement;
        pendingMovement = 0;
    }
    return unixResult;
}

static int cursorCalls;
static int cursorResult = 1;

static int GetCursorPos(POINT *position) {
    cursorCalls++;
    position->x = 0;
    position->y = 0;
    return cursorResult;
}

#include "frame_input_pump.h"

int main(void) {
    const char *values[] = {NULL, "0", "10", "1", "1"};
    int passed = 0;
    for (unsigned int index = 0; index < 5; index++) {
        if (values[index]) {
            setenv("D9MT_PUMP_INPUT", values[index], 1);
        } else {
            unsetenv("D9MT_PUMP_INPUT");
        }
        cursorCalls = 0;
        unixCalls = 0;
        unixResult = index == 4 ? -1 : 0;
        cursorResult = 0;
        d9mtPumpFrameInput();
        const int expected = index >= 3 ? 1 : 0;
        const int matches =
            cursorCalls == (index == 4) && unixCalls == expected;
        passed += matches;
        printf("%s frame pump case %u\n", matches ? "PASS" : "FAIL", index);
    }
    int checks = 5;
#define CHECK_TIMER(condition, label)                                          \
    do {                                                                       \
        const int matches = (condition);                                       \
        passed += matches;                                                     \
        checks++;                                                              \
        printf("%s %s\n", matches ? "PASS" : "FAIL", label);                   \
    } while (0)

    setenv("D9MT_PUMP_INPUT", "1", 1);
    unixResult = 0;
    pendingMovement = 12;
    deliveredMovement = 0;
    d9mtPumpFrameInput();
    CHECK_TIMER(deliveredMovement == 12 && !pendingMovement,
                "fresh cached cursor still services pending driver motion");
    pendingMovement = 7;
    d9mtPumpFrameInput();
    CHECK_TIMER(deliveredMovement == 19 && !pendingMovement,
                "repeated frames service newly queued motion");
    setenv("D9MT_PUMP_INPUT", "0", 1);
    pendingMovement = 5;
    d9mtPumpFrameInput();
    CHECK_TIMER(deliveredMovement == 19 && pendingMovement == 5,
                "disabled pump leaves queued motion untouched");
    pendingMovement = 0;

    int firstWindow;
    int secondWindow;
    HWND timerWindow = NULL;
    UINT_PTR timer = 0;
    unsetenv("D9MT_PUMP_INPUT");
    d9mtUpdateInputPumpTimer(&firstWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 0 && timer == 0, "disabled timer stays absent");
    setenv("D9MT_PUMP_INPUT", "1", 1);
    d9mtUpdateInputPumpTimer(&firstWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 1 && timerWindow == &firstWindow &&
                    timerInterval == 20 && timer == 1,
                "enabled timer targets window thread");
    unixCalls = 0;
    timerCallback(timerWindow, 0, timer, 0);
    CHECK_TIMER(unixCalls == 1, "timer pumps without Present");
    d9mtUpdateInputPumpTimer(&firstWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 1 && killCalls == 0, "frames reuse timer");
    d9mtUpdateInputPumpTimer(&secondWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 2 && killCalls == 1 &&
                    timerWindow == &secondWindow,
                "window change removes old timer");
    setenv("D9MT_PUMP_INPUT", "0", 1);
    d9mtUpdateInputPumpTimer(&secondWindow, &timerWindow, &timer);
    CHECK_TIMER(killCalls == 2 && !timer && !timerWindow,
                "disabling removes timer");
    setenv("D9MT_PUMP_INPUT", "1", 1);
    timerResult = 0;
    d9mtUpdateInputPumpTimer(&firstWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 3 && !timer && !timerWindow,
                "failed registration leaves retry possible");
    timerResult = 1;
    d9mtUpdateInputPumpTimer(&firstWindow, &timerWindow, &timer);
    CHECK_TIMER(timerCalls == 4 && timer == 1,
                "next frame retries registration");
    d9mtUpdateInputPumpTimer(NULL, &timerWindow, &timer);
    d9mtStopInputPumpTimer(&timerWindow, &timer);
    CHECK_TIMER(killCalls == 3 && !timer && !timerWindow,
                "null window and repeated cleanup are safe");
    printf("%d passed, %d failed\n", passed, checks - passed);
    return passed == checks ? 0 : 1;
}
