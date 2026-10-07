# Looking Glass: static quilt capture prototype

This is a capture experiment, not an integrated Looking Glass video driver.
It renders 48 views from one emulated frame into an 8-column, 6-row quilt.
No Bridge SDK, display calibration or Looking Glass hardware is required to
build the core or capture the quilt. Viewing on the device requires the
manufacturer's software and a compatible display; that has not been tested.
The existing SBS PR is unchanged; this work is on a separate branch.

## Capture

1. Build this branch with `make -f Makefile.libretro -j4 platform=osx ARCHFLAGS=`.
   The arm64 library is `genesis_plus_gx_libretro.dylib` in the repository root.
2. Load it in RetroArch with your own game:

   ```sh
   /Applications/RetroArch.app/Contents/MacOS/RetroArch \
     -L /private/tmp/genesis-looking-glass/genesis_plus_gx_libretro.dylib \
     "/path/to/your/Sonic.md"
   ```

3. Disable video shaders and CPU video filters. In **Core Options → Video**,
   select **Stereo 3D → Looking Glass Quilt (8x6, 48 views)**. Keep the existing
   layer depth controls. They now specify displacement at the two extreme
   views; intermediate offsets are interpolated. **Swap Eyes** reverses all
   48 views.
4. Turn **Settings → Video → GPU Screenshot** off. Use **Quick Menu → Take
   Screenshot** after a complete frame. Menu paths vary by RetroArch version.
   Capture the native core framebuffer, not the scaled window or OS screen.
   For woven interlace, allow both fields to update before pausing/capturing.
5. Prepare the resulting PNG:

   ```sh
   python3 extras/stereo3d/prepare_quilt.py /path/to/screenshot.png --aspect 1.333333
   ```

   The script copies the PNG under the manufacturer's quilt filename convention,
   such as `screenshot_qs8x6a1.333333.png`. It validates native dimensions and
   leaves pixels unchanged. `--aspect` is the intended display aspect of ONE
   view, not the whole quilt. Use the ordinary core aspect ratio instead of
   4:3 if exact original proportions are desired. Existing files are not overwritten.

For 320x224 without borders, the native quilt is **2560x1344**; for 256x224 it
is **2048x1344**. Interlace doubles each tile's height. With maximum borders
and height, bounds are 2784x3456, RGB565 pitch 5568 bytes. The larger buffer
is allocated only when quilt mode is first selected and freed at core deinit.
SBS uses a separate 696x576 buffer (1392-byte pitch), including H40 overscan.

## Display through Looking Glass software

Install Looking Glass Bridge and follow the official Python SDK instructions:

- [Bridge Python SDK and installation](https://github.com/Looking-Glass/Bridge-Python-SDK)
- [Quilt layout and filename convention](https://lookingglassfactory.com/tutorial/what-is-a-quilt)

In the SDK's configured environment, use its example:

```sh
python -m bridge_python_sdk.Examples.DisplayQuilt /path/to/screenshot_qs8x6a1.333333.png
```

View 0 is in the bottom-left tile. Views advance left to right across each
row, then bottom to top; view 47 is at the top right. The output is a generic
8x6 quilt, not a device-specific optimized profile. This grid is commonly
used for Portrait, but the aspect metadata here describes the Genesis image,
not the portrait panel. SDK/viewer behavior, letterboxing and calibration
must be checked on the actual model. This is not the manufacturer's recommended
3360x3360 Portrait capture resolution; each view retains the game's native
pixels. Do not load the SBS row-interleaving shader for a quilt.

## Limitations and verification

All original stereo limitations apply. The scene consists of flat depth layers,
not volumetric geometry. Offsets are rounded to whole game pixels; at depth 2,
there are only five distinct offsets across 48 views. Expect repeated views
and stepped motion parallax. Larger depth increases the effect and possible
artifacts. The displacement is artistic and is not calibrated against the SDK's
camera/view cone. A physical test may require reversing views or depth signs.

The CPU composes all 48 views every frame while capture mode is enabled, without
advancing emulation or replaying VDP side effects. It is intentionally slow;
turn it off after capturing. For live output, use the separate experimental [GPU viewer](LIVE_LOOKING_GLASS.md).
The CPU capture mode itself has no live Bridge connection or GPU synthesis. If the quilt allocation
fails, the core falls back to ordinary output; the preparation script rejects
its dimensions.

Build and synthetic tests do not establish optical correctness on a display.
No ROM or game was loaded during development. The user subsequently tested RetroArch screenshots and displayed captured quilts
on the physical panel; live playback requires a separate test.

Additional checks performed on this branch: the macOS arm64 build and existing
SBS synthetic checks passed again for both renderers. AddressSanitizer checks
covered all 48 tile positions, reversed view ordering, ±16 interpolation,
blanking and writes to the final row of the maximum-size buffer. Geometry tests
covered ordinary/SBS/quilt transitions at H32/H40, borders and interlace.
The PNG preparation tool was checked for unchanged pixel/file data, metadata
naming, rejection of scaled/SBS images and refusal to overwrite existing files.
Linux/Windows builds were not checked.

The user has since displayed captured Sonic quilts using Bridge on an Apple M4
and a 1440x2560 Looking Glass panel. This does not validate the new live frontend.
On macOS, `display_quilt_macos.py` runs the SDK capture viewer with the explicit
OpenGL 4.1 core context used by the working cube example.
