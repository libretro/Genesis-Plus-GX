# Live Looking Glass in RetroArch (experimental, macOS)

The native libretro adapter loads the modified Genesis Plus GX core inside
RetroArch. One emulation pass produces a compact layer atlas. The adapter sends
that atlas through a shared memory file to a separate Python Bridge worker.
The worker composes device-profile views on the GPU and displays the quilt.
Audio, controller input, save states and SRAM remain under RetroArch's control.
The worker never loads a ROM or runs the emulator.

Bridge and RetroArch contain conflicting SDL/Cocoa classes on macOS. Keeping
Bridge in a separate process avoids loading those duplicate classes into
RetroArch. The worker uses the same GLFW OpenGL 4.1 context and compositor as
the standalone viewer. The adapter has its own offscreen native Cocoa context
for RetroArch's middle-view preview and restores the caller's context after
rendering. RetroArch can keep its Metal driver. No RetroArch source is modified.

The shared buffer holds the latest complete atlas; the worker skips older frames
if rendering falls behind. A sequence counter rejects a frame copied during a
producer update. Emulator timing and audio do not wait for the worker.

## Build and launch on macOS

From the repository root, build the ordinary core and adapter:

```sh
sh extras/stereo3d/build_retroarch_looking_glass.sh
```

Close the standalone live viewer and other Bridge examples, then launch:

```sh
sh extras/stereo3d/run_retroarch_looking_glass.sh \
  "/absolute/path/to/your/game.md"
```

The launcher uses the installed `/Applications/RetroArch.app`, Bridge 2.6.3,
the core in this checkout and the existing RetroArch preferences copied into an
isolated session config. Initial depth settings are B=0, A=8, sprites=12 and
Swap Eyes enabled. It keeps a dedicated core-options file under
`extras/stereo3d/.retroarch-live/`; subsequent launches preserve your depth edits.
It disables shaders and CPU video filters for this session. The worker requires Python, GLFW, NumPy, PyOpenGL and the Bridge Python SDK,
using the existing `~/looking-glass-venv/bin/python` by default. Set
`GENESIS_LOOKING_GLASS_PYTHON` to use another prepared Python environment.
Bridge supplies its own runtime libraries. The launcher starts the worker first,
waits for readiness and stops only that worker when RetroArch exits.

Configure different locations with `RETROARCH_BIN`, `RETROARCH_CONFIG` and
`GENESIS_LOOKING_GLASS_BRIDGE`. The launcher exports the underlying core and shader
paths for the adapter. Launching the adapter directly without those environment
variables will not load the underlying core.

In Quick Menu → Core Options, use Stereo 3D → Full Side-by-Side to enable this
adapter's live output. The adapter translates enabled stereo output to the layer
stream internally. Turning Stereo 3D off returns to the ordinary core's video.
Adjust Plane A, Plane B, sprites and Swap Eyes there; no capture/restart is needed
for depth changes. The Window plane stays at screen depth.

Use RetroArch's configured hotkey to open Quick Menu (usually F1, or Fn+F1 on a
Mac); keyboard/gamepad mappings, pause, save/load state and SRAM use the standard
RetroArch controls. Changes to the main RetroArch config should be made in a
normal RetroArch session. This launch uses a copy so its video/filter/core-options
settings do not replace your normal configuration. SRAM and state directories
follow your normal settings; RetroArch may create a folder named
`Genesis Plus GX Looking Glass`. Existing saves under another core's folder may
need to be copied explicitly; the adapter does not move or migrate them.

## Supported scope and verification

This adapter currently supports macOS desktop and software-rendered games in our
modified Genesis Plus GX core. Other systems pass through ordinary core output.
All Genesis stereo limitations remain: flat depth per plane/sprite group,
whole-pixel depth rounding, and flat fallback for unsupported VDP modes.
The Looking Glass picture contains the game; Quick Menu and overlays appear in
RetroArch's ordinary window. A single middle view is read back for that window;
the multiview quilt remains on the worker GPU; only the layer atlas crosses processes. GPU screenshots capture the normal
RetroArch preview, not a complete quilt. The separate CPU capture mode remains
available through the unwrapped core for quilt screenshots.

Verified on macOS arm64: C89-compatible adapter/core build; all 66 synthetic
views and the preview matched a CPU reference, including vertical orientation;
OpenGL host-context restoration; changing geometry; input/audio callback
forwarding; reset; save-state round trips; SRAM access and cheat/region forwarding.
An installed RetroArch 1.22.2 run used Metal and CoreAudio for 120 synthetic
frames, saved the test SRAM successfully and unloaded cleanly. That integration
test used a synthetic core and a mock Bridge, not a game or physical display.
No ROM was loaded by the developer. Windows/Linux adapter builds are unsupported;
RetroArch source and its CI-covered files were not changed.

The user confirmed smooth Sonic, clean audio and correct depth in the standalone
live viewer on 2026-10-06. The user also confirmed working live Sonic video and audio through the
isolated RetroArch adapter on 2026-10-06. Quit and relaunch after a Bridge initialization/draw
failure; detailed errors appear in the RetroArch message/log.

The native Cocoa context fix was additionally checked with the real Bridge
2.6.3: the calibrated 1440x2560 panel window and 11x6 grid initialized, three
synthetic frames were accepted, and the preview contained valid pixels. This
checks the integration API, not the optical appearance of Sonic. The launcher
now writes diagnostics to `.retroarch-live/retroarch.log`.

The corrected adapter also completed a 30-frame run inside installed RetroArch
with the real Bridge and a synthetic core: the calibrated panel window
initialized, the live grid was accepted, test SRAM was saved and both libraries
unloaded cleanly. This API test did not establish physical output; see the subsequent user test below.

The launcher raises an inherited soft open-file limit below 4096 to 4096 for
the launched process. A user run exhausted its file descriptors (`dlopen`
returned errno 24), preventing Bridge from loading. This adjustment affects
only the launcher and child process; it does not change global macOS settings.

The launcher keeps RetroArch windowed for the experimental two-window output,
keeping the game viewport on the normal desktop. The adapter explicitly
shows the Bridge window and sends initialization/failure messages through
RetroArch's logging API as well as stderr.

The in-process adapter initialized and accepted frames but did not produce a
visible panel window in the user test. Initialization alone does not verify
physical output. The isolated worker is the current launch path. The user subsequently
confirmed both Sonic video and audio on the physical panel. Diagnostics are separate:
`.retroarch-live/retroarch.log` for RetroArch and `.retroarch-live/bridge.log` for
the worker. The worker reports `Ready: 11x6` (device dependent),
`First layer frame received` and a running received-frame count.

The isolated path passed a synthetic 120-frame test with real Bridge 2.6.3:
shared atlas bytes matched the producer, the middle preview matched the CPU
reference, and callback/state/context checks passed. The launcher started the
worker, received its readiness signal and first frame, and removed its temporary
stream on exit. These tests loaded no ROM and do not verify optical output.
