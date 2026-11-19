# Live Looking Glass in RetroArch (experimental, macOS)

The optional libretro adapter loads this modified Genesis Plus GX core inside
RetroArch. One emulation pass produces a compact layer atlas. A GPU compositor
builds the multiview quilt while audio, input, save states and SRAM remain under
RetroArch's control. No RetroArch source changes are required.

## Worker output (default)

```sh
sh extras/stereo3d/build_retroarch_looking_glass.sh
sh extras/stereo3d/run_retroarch_looking_glass.sh "/absolute/path/to/game.md"
```

The isolated Python worker requires GLFW, NumPy, PyOpenGL and the Bridge Python
SDK. Its default interpreter is `~/looking-glass-venv/bin/python`; override it
with `GENESIS_LOOKING_GLASS_PYTHON`. The launcher waits for readiness and removes
its shared atlas stream and worker on exit. Only the compact atlas crosses
processes; the quilt is composed on the GPU.

## Direct Metal output (opt-in)

Build against an external Bridge SDK header directory containing `bridge.h`:

```sh
GENESIS_LOOKING_GLASS_SDK_HEADERS="/path/to/BridgeRuntime" \
  sh extras/stereo3d/build_retroarch_looking_glass.sh
GENESIS_LOOKING_GLASS_MODE=metal \
GENESIS_LOOKING_GLASS_MAX_FPS=30 \
  sh extras/stereo3d/run_retroarch_looking_glass.sh "/absolute/path/to/game.md"
```

Metal mode needs the native adapter and Bridge runtime, without Python or a
worker. Building without SDK headers preserves worker/legacy GL support and
reports an error if Metal mode is selected. The physical test used Bridge 2.6.3
and a separate macOS arm64 Metal/Cocoa RetroArch build without SDL. Select a
custom executable with `RETROARCH_BIN`. SDL-free RetroArch is the tested setup,
not an established requirement of the Metal API.

`GENESIS_LOOKING_GLASS_MAX_FPS` optionally caps quilt submissions between 1 and
120 Hz. It does not change emulation or audio rates. Unset it for submission on
every frame. On the tested Apple M4 setup, the user confirmed normal Sonic audio
with the cap at 30 Hz; full-rate quilt composition caused audio interruptions.
Lower panel update rates reduce motion smoothness. This is not a CoreAudio fix.
Set `GENESIS_LOOKING_GLASS_PROFILE=1` for periodic GPU/SDK timing logs.

The backend allocates one SDK-owned RGBA8 IOSurface Metal texture at the device's
quilt dimensions. A compute shader writes the views directly into that texture,
without an intermediate quilt copy. Source geometry changes do not reallocate
it. The producer waits for GPU completion, restores the SDK's GL context,
shows/pumps the window and submits the quilt once. The launcher disables
pause-on-focus-loss in Metal mode.

The legacy GL path (`GENESIS_LOOKING_GLASS_MODE=direct`) remains experimental:
SDK calls succeeded, but subsequent physical tests showed no image. Do not
interpret successful initialization as proof of panel output.

## Configuration and controls

The launcher defaults to `/Applications/RetroArch.app`, Bridge 2.6.3 and the core
in this checkout. Override locations with `RETROARCH_BIN`, `RETROARCH_CONFIG`
and `GENESIS_LOOKING_GLASS_BRIDGE`. It copies the normal RetroArch configuration
into an isolated session, selects Metal, disables shaders/CPU filters and keeps
the frontend windowed. It raises the child's soft file limit to 4096 when needed.
Installed applications and global settings are not changed.

Initial depth settings are B=0, A=8, sprites=12 and Swap Eyes enabled. Subsequent
launches preserve edits in `extras/stereo3d/.retroarch-live/core-options.cfg`.
In Quick Menu → Core Options, Full Side-by-Side enables the adapter's layer
stream internally; disabling Stereo 3D restores ordinary core video. Window
stays at screen depth. Normal hotkeys, controllers, pause and saves still apply.
SRAM/state paths follow your settings, possibly under `Genesis Plus GX Looking
Glass`; existing saves are not migrated automatically.

The Looking Glass displays the game. Menus and overlays appear in RetroArch's
window, which reads back a middle view. Metal mode composes only three GL views
for that preview. The multiview quilt stays on the GPU. Static CPU quilt capture
is not provided. Other systems retain ordinary core output; Genesis limitations
include uniform depth per plane/sprite group, whole-pixel rounding and flat
fallback for unsupported VDP modes.

## Validation and limits

The macOS arm64 core and C89 adapter builds passed. Twelve synthetic Metal
quilts matched a CPU reference byte-for-byte, including geometry changes, eye
swap, shadow/highlight, flat fallback, left blanking and Window selection. The
fixture checked one target allocation, one SDK submission per rendered frame,
release and shutdown. A separate check verified capped submissions resume after
the interval. Existing GL tests checked host-context/viewport restoration and
middle-view pixels, including when a mock SDK cleared the current context.

The user confirmed moving synthetic frames on the physical panel in direct
Metal mode. A 3600-frame synthetic run completed and closed the SDK. The user
subsequently confirmed Sonic output and normal audio with the 30 Hz cap.
Developer-run checks used synthetic data and loaded no ROM. The worker also
passed a 120-frame real-Bridge synthetic test with shared-atlas verification,
readiness and cleanup checks.

Testing covers one calibrated 1440×2560 Looking Glass using an 11×6 quilt.
The SDK startup `GL_INVALID_ENUM` diagnostic remains. Producer completion is not
a documented guarantee of SDK consumer completion. Windows/Linux adapters are
unsupported. Quit and relaunch after a Bridge failure. Logs are in
`.retroarch-live/retroarch.log` and, for worker mode, `.retroarch-live/bridge.log`.

Run the synthetic Metal fixture with an external SDK header directory and a
Python environment containing NumPy (no Bridge runtime or ROM is needed):

```sh
GENESIS_LOOKING_GLASS_SDK_HEADERS="/path/to/BridgeRuntime" \
GENESIS_LOOKING_GLASS_PYTHON="/path/to/python" \
  sh extras/stereo3d/tests/test_metal.sh
```
