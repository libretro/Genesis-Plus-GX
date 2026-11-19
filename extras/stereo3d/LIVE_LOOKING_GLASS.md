# Experimental live Looking Glass viewer

This standalone libretro frontend runs Genesis Plus GX once per frame. The core
exports Plane A, Plane B, flattened sprites, Window, the per-line RGB565 palette
and the original priority/shadow/highlight lookup tables as a 2048x704 integer
atlas. OpenGL composes separate views before final mixing. Looking Glass Bridge
receives the resulting GPU quilt texture directly; screenshots are not involved. RetroArch is not required for this viewer. For RetroArch input, audio and saves,
use the [native libretro adapter](RETROARCH_LOOKING_GLASS.md).

The GPU texture uses the same top-down row layout as the SDK PNG uploader;
the preview compensates for this texture orientation.

The grid comes from Bridge's device profile (11x6 on the user's attached panel).
Views retain native game resolution. Bridge applies device calibration and fits
the supplied 4:3 content aspect; the viewer does not hardcode panel calibration.
Window stays at screen depth. ±16 game pixels of source data allow shifted layers
to reveal lower layers. Sprites are evaluated once; emulation time, sprite status
and IRQs are not replayed for additional views.

## Build and dependencies

Build the core in the same checkout as the viewer:

```sh
make -f Makefile.libretro -j4
```

The tested environment is macOS arm64, Apple M4, OpenGL 4.1 core, Python 3.14,
GLFW, PyOpenGL, NumPy, SDL2 and Looking Glass Bridge 2.6.3. Use the existing
`looking-glass-venv` containing `bridge_python_sdk`, or install the manufacturer's
Bridge Python SDK and its dependencies in a virtual environment. SDL2 must be
available as a shared library; on macOS Homebrew's `sdl2` package supplies it.
Use `--sdl /opt/homebrew/lib/libSDL2.dylib` if automatic discovery fails.
Other platforms have not been tested. Use `--core` for a differently named library.

## Run on this machine

Close RetroArch and other Bridge examples first. Connect the display and ensure
Bridge detects it. Activate the existing environment, then supply your own ROM:

```sh
source ~/looking-glass-venv/bin/activate
python /private/tmp/genesis-looking-glass/extras/stereo3d/live_viewer.py \
  "/absolute/path/to/your/Sonic.md" --b 0 --a 8 --sprites 12 --swap
```

Replace the quoted path with the actual local ROM path. The default core path is
`genesis_plus_gx_libretro.dylib` at the root of this checkout; `--core PATH`
overrides it. `--preview-only` runs a middle-view preview without Bridge.
The GPU Layer Stream option is selected automatically. Do not select it for
ordinary RetroArch display: the encoded atlas is not a picture.

Controls require focus on the preview window:

| Key | Action |
| --- | --- |
| Arrows | Direction pad |
| Z / X / C | Genesis A / B / C |
| Enter | Start |
| Space | Pause/resume |
| F2 | Reverse view order |
| F3 | Reset console |
| Escape or Ctrl+C | Exit |

Mac function keys may require Fn. If depth appears inverted, press F2 or remove
`--swap`. Offsets describe the displacement of each extreme view: the total
extreme-view difference is twice the value. Intermediate views use whole-pixel
rounding. Change `--a`, `--b`, `--sprites` on the next launch to adjust depth.

Audio uses SDL2 stereo signed-16 samples with approximately 60 ms of initial
buffering. The console prints emulated FPS and queued audio milliseconds every
five seconds. NTSC should stay near 59.9 FPS and PAL near 49.7 FPS; persistent
slower execution or a draining queue indicates that real-time audio is not being
sustained. A synthetic GPU timing is not a complete emulation benchmark.

## Limitations and verification

Only Mega Drive/Genesis Mode 5 is supported. Enhanced column scrolling and
non-Mode-5 lines retain the existing flat fallback. All sprites share one depth;
all parallax bands within a plane share its depth. Layer occlusion cannot become
volumetric geometry. Depth offsets are artistic and not calibrated camera angles.
The frontend currently has keyboard input, reset and pause; it does not implement
gamepads, save states, SRAM persistence or RetroArch menu integration.

Checked without loading a ROM: macOS arm64 core build; SBS AddressSanitizer tests; GPU RGB output against CPU rendering for 32 synthetic
frames covering H32/H40, borders, Window, shadow/highlight, reversed/negative
depths, interlace tile patterns and blanked lines; SDL2 silent audio queue ABI.
GPU and CPU colors matched exactly. A synthetic 66-view GPU composition averaged
about 2 ms/frame on Apple M4. A stub libretro core exercised frontend callbacks,
frame pacing, GPU composition and silent audio without running a game.

On 2026-10-06, the user confirmed smooth live Sonic playback, clean audio and
correct depth on the attached Looking Glass after the vertical orientation fix,
using B=0, A=8, sprites=12 and reversed views. This confirms that setup, not every
device or game. The user also confirmed live video and audio through the RetroArch adapter.

## Layer atlas layout

The stream is experimental and uses RGB565 libretro transport as unsigned 16-bit
data, not as display colors. Source rows run from top to bottom. Each row contains
four 352-pixel slots (A, B, sprites, Window), covering game x=-16..335. Window's
bit 7 marks coverage, including transparent Window pixels. Slots preserve the
core's palette/priority indices. The next 256 values are the real RGB565 palette.
At x=1664..1673, metadata records output width, output height, border width,
A/B/sprite depths biased by 16, view reversal, shadow/highlight, flat fallback
and left-column blanking. Rows 576..703 pack the four 65536-entry lookup tables
for ordinary backgrounds, ordinary sprites, shadow/highlight backgrounds and
shadow/highlight sprites. The stream geometry/pitch is always 2048x704/4096 bytes;
the metadata carries the changing game geometry.
