# Aion d9mt experiment

This directory contains the Wine 11 adapter and d9mt patches used for the
isolated Aion test. It is experimental and is bundled as ReRun's Metal D9MT option.

The Wine adapter exports the DXMT v0.80 function table, translates its older
window-data layout, creates a Wine client surface when needed, and retains that
surface until the Metal view is released. The companion loader exposes the
isolated driver through `RTLD_GLOBAL`, because Wine normally uses `RTLD_LOCAL`.
The patch also adapts the companion PE shim to Wine 11's Unix dispatcher and
advertises storage writes for half-float bump-map conversion destinations.

## Camera input and frame limit (2026-10-01)

The user verified smooth rotation with the direct native input pump. Cached
`GetCursorPos` polling and a normal message wait were insufficient: AppKit
received about 60 drag events/second, but the Wine thread delivered about one
combined movement every 1.25 seconds. The frontend now calls the existing
reentry-guarded driver queue pump through companion Unix-call entry 5.
`direct-input.patch` adds the entry to both native and WOW64 tables;
`input_dispatch.h` resolves the exported driver function once. This does not
synchronously drain AppKit.

D9MT launches enable `D9MT_PUMP_INPUT=1`, `D9MT_CAPTURE_MOUSE=1`, and
`D9MT_RELATIVE_MOUSE=1`. The focused right-button drag selects the event-tap
clipping path (`UseConfinementCursorClipping=n` only in the D9MT prefix).
`relative-mouse.patch` uses warp-corrected deltas and preserves them across
recenter calls; the prior clip is restored on release. Other mouse paths retain
normal Wine behavior. Native diagnostic counters were removed from the bundle.

ReRun's Aion tab has a persisted **Limit to 60 FPS** switch for Metal D9MT,
enabled by default. It selects `DXVK_FRAME_RATE=60`; turning it off removes the
cap without disabling the camera fixes. Changes apply on the next Play.
The verified synchronization setting is `WINEMSYNC=0` for D9MT.

The current focused checks pass 54 tests, 0 failures: adapter 12, frame pump 17,
capture 8, motion policy 6, native dispatch 3, patch builder 6, cursor warp 2.
ReRun's package passes 94 tests, 0 failures. The signed app and clean renderer
bundle build successfully. Detailed artifacts and rollback backups are in
`.build/d9mt-test/checkpoints/camera-lag-20261001/`.

## Executable data buffer control

Renderer-owned Metal buffers use native read/write permission without
execute permission by default. Wine can add execute permission when the
client's DEP policy is disabled, causing pathological memory-write overhead
under Rosetta. During the reported slowdown, the game thread was copying
vertex data and shader constants while the submission thread was mostly idle,
and the client accumulated approximately 123,000 memory faults per second.
The destination Metal buffer mappings were readable, writable and executable.
After removing execute permission from those buffers, the user confirmed
normal gameplay on 2026-10-04. The exact Rosetta tracking mechanism remains
an inference from the samples and protection comparison.

The fix changes only renderer-owned buffer allocations, after Metal
registration and before their first write. It preserves read/write access
and the client's DEP policy. `D9MT_DATA_BUFFER_NX=0` disables it for comparison.
A matching frontend and companion build is required. Do not replace mapped
renderer libraries during a live game session.

## Build

Requirements: Xcode and its Metal compiler, `mingw-w64`, `glslang`, `bison`,
`flex`, Python 3, and git. The existing neo773/d9mt source and DXMT v0.80 bridge
binaries are under `.build/d9mt-test/d9mt-main`.

From the repository root:

```bash
bash Apps/ReRun/Tools/D9MT/build-driver.sh
python3 Apps/ReRun/Tools/D9MT/build-companion.py .build/d9mt-test/d9mt-main
cd .build/d9mt-test/d9mt-main
# Upstream's object cache does not track included headers. The timer patch
# changes the swapchain class layout, so rebuild all frontend consumers.
touch vendor/dxvk/src/d3d9/*.cpp
bash scripts/build-dxvkfe.sh
```

`build-driver.sh` pins Wine 11.0 to
`db11d0fe6a169c457e23d007e20404643d067aa8`. It builds only the Unix Mac driver
and its build dependencies. The companion helper applies `d9mt.patch` and `compute-dispatch.patch`, `depth-stencil-cache.patch`, and
also applies `frame-input.patch` and `timer-input.patch`, and installs `frame_input_pump.h` into the
frontend sources, then stops before upstream's CrossOver installation steps. Patch application uses
`--forward` and verifies already-applied patches without reversing them.

The isolated launcher at `.build/d9mt-test/run.py` uses a copy of ReRun's Wine
11 engine and a dedicated prefix. With that test stopped, copy the driver to
`.build/d9mt-test/engine/lib/wine/x86_64-unix/winemac.so`. The launcher installs
the companion and D3D9 components into its own engine/prefix at startup.
It sets `D9MT_MAC_DRIVER_PATH` to that isolated driver, which the companion
loads with global visibility. It also sets `D9MT_PUMP_INPUT=1`, enabling a
guarded event pump when Aion polls the cursor. The pump defaults to off and
skips recursive event processing. `cursor-warp.patch` also invokes this pump
before `SetCursorPos`, allowing queued movement to be delivered before Wine's
native warp discards it. Camera-lag improvement still needs user verification.
Keep ReRun's installed engine unchanged.

```bash
python3 .build/d9mt-test/run.py --debug
python3 .build/d9mt-test/run.py --stop
```

## Checks

```bash
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  Apps/ReRun/Tools/D9MT/wine_bridge_tests.c \
  -o .build/d9mt-test/wine-bridge-tests
.build/d9mt-test/wine-bridge-tests
python3 Apps/ReRun/Tools/D9MT/test_build_companion.py
python3 Apps/ReRun/Tools/D9MT/test_cursor_warp.py
python3 Apps/ReRun/Tools/D9MT/test_depth_stencil_cache.py
python3 .build/d9mt-test/test_launcher.py
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  Apps/ReRun/Tools/D9MT/frame_input_pump_tests.c \
  -o .build/d9mt-test/frame-input-tests
.build/d9mt-test/frame-input-tests
```

The portable adapter suite contains 12 checks, including event-pump opt-in
and reentrancy. The patch-builder suite contains 3 tests and the launcher
suite contains 3 mocked tests. All 18 passed; 0 failed. The existing ReRun
TCP relay suite also passed 3 tests; 0 failed.

`render_probe.c` is a Windows integration probe for device
creation, `X8L8V8U8` texture creation, clear, and present. Its optional first
argument is the Windows output-log path. A successful `Present` return alone
is insufficient: check that the frontend log reports a created Metal surface
and swapchain and validate visible pixels.

The patched bridge reached Aion's login scene at 1920x1080; the Metal HUD showed
about 300–360 FPS. Terrain colors remain incorrect. Adding half-float storage
capability eliminated texture-allocation errors but did not correct those
colors. This is not yet a gameplay benchmark or a fully compatible renderer.
See `Apps/ReRun/docs/aion-metal-renderer.md` for the experiment history.

Keyboard input now works, and the user confirmed entering the game. The earlier
restart crashes were caused by macOS `patch` automatically reversing the Unix
dispatcher patch on repeated builds; the builder now prevents this.

The isolated launcher bypasses ReRun's server setup. For the local container
stack, it needs TCP relays on localhost ports 2106 (login), 7777 (game) and
10241 (chat). Without the login relay, Aion reports that it cannot connect to
the authorization server even while the server containers are running.
ReRun's existing `TCPRelay.swift` supplied the temporary test relays; IPs
must be refreshed when containers restart.

Waterfall rendering and terrain colors need another Aion play-test. The GPU
sampler test passes all 8 slots with `D9MT_ASYNC=0`; its first asynchronous
run read the clear color before compilation finished. Direct half-float
uploads also pass all 8 samples.

`compute-dispatch.patch` fixes X8L8V8U8 conversion: winemetal v0.80 keeps
threadgroup dimensions only within one encodeCommands call. d9mt previously
sent SetPSO and Dispatch separately, so Dispatch received zero dimensions.
The patch sends both in one chain. The dedicated
`texture_conversion_probe.c` failed 8/8 checks with the old renderer and
passes 8/8 with the fix, including signed channel values; Metal API validation
no longer reports a zero-sized threadgroup. The fixed frontend is built, but
has not yet been loaded by Aion. The next isolated launch installs it.

Full reboot/resume instructions, current runtime state and saved artifacts:
[`../../docs/aion-d9mt-session-handoff.md`](../../docs/aion-d9mt-session-handoff.md).

The evening play-test reached the world, but the user reported no keyboard or
mouse response. The character was visible and the Java server logged world
entry. This is a new gameplay-input limitation of the cursor-poll workaround,
not a confirmed recurrence of the older invisible-character packet bug.

`frame-input.patch` now polls `GetCursorPos` on the application's `Present`
thread before the swapchain lock, gated by the same `D9MT_PUMP_INPUT=1` opt-in.
This reuses the existing driver's guarded event pump even when the gameplay
loop stops polling the cursor itself. Five portable checks cover absent,
disabled, invalid and enabled flags, including a failed cursor query. The
frontend rebuild and eight conversion pixel checks pass. The new build is
installed only in the isolated prefix. The user subsequently confirmed that
input now works inside the game. Keep the per-frame pump and synchronous
launch settings together for reproduction; the underlying event-wait cause
and asynchronous startup reliability remain unresolved.

Cmd+Tab subsequently left Aion unresponsive after a brief recovery. The game
window still responded to `WM_NULL`, and native samples showed the main
Windows thread waiting in `GetMessage`; the CS/watcher threads were waiting
rather than blocked inside a Metal call. Manually posting activation messages
gave only temporary recovery. This is evidence of a focus-resume/input issue;
it does not establish its underlying Wine cause.

`timer-input.patch` adds a 20 ms window timer that uses the same guarded cursor
pump on the window owner's thread, even when Aion stops calling `Present`.
It reuses one timer, replaces it when the destination window changes, and
removes it when disabled or when the swapchain is destroyed. The portable
frame-pump suite now passes 14 checks, including timer callbacks without
`Present`, window replacement, cleanup and registration-failure retries.
The timer build is installed. Native focus switching away and back resumes
login rendering. The user also confirmed input and in-world Cmd+Tab recovery.
Long-session stability still needs testing; camera movement exposes intermittent
terrain corruption that later recovers. Current
focused suites pass 40 checks with 0 failures, including 8 GPU conversion cases.
