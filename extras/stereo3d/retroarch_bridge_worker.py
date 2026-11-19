#!/usr/bin/env python3
"""Receive layer frames from RetroArch without loading Bridge into its process."""
import argparse
import mmap
import os
from pathlib import Path
import signal
import struct
import time

import glfw
import numpy as np
from OpenGL import GL
from live_viewer import Renderer
from bridge_python_sdk.BridgeApi import BridgeAPI, PixelFormats

SIZE = 64 + 2048 * 704 * 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stream', type=Path)
    parser.add_argument('--max-frames', type=int, default=0)
    args = parser.parse_args()
    stopping = False

    def stop(*_):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    window = bridge = renderer = None
    with args.stream.open('w+b') as file:
        file.truncate(SIZE)
        with mmap.mmap(file.fileno(), SIZE) as shared:
            struct.pack_into('6I', shared, 0, 0x474c4741, 0, 0, 0, 0, 0)
            try:
                if not glfw.init():
                    raise RuntimeError('GLFW initialization failed')
                glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 4)
                glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 1)
                glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
                glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, glfw.TRUE)
                glfw.window_hint(glfw.VISIBLE, glfw.FALSE)
                window = glfw.create_window(32, 32, 'RetroArch Bridge worker', None, None)
                if not window:
                    raise RuntimeError('OpenGL window creation failed')
                glfw.make_context_current(window)
                glfw.swap_interval(0)
                bridge = BridgeAPI(library_path=os.environ.get("GENESIS_LOOKING_GLASS_BRIDGE"))
                if not bridge.initialize('RetroArchGenesisWorker'):
                    raise RuntimeError('Bridge initialization failed')
                panel = bridge.instance_window_gl(-1)
                _, _, _, columns, rows = bridge.get_default_quilt_settings(panel)
                grid = (int(columns), int(rows))
                if min(grid) < 1 or not 2 <= grid[0] * grid[1] <= 128:
                    raise RuntimeError('Unsupported device grid')
                bridge.show_window(panel, True)
                glfw.make_context_current(window)
                renderer = Renderer(grid)
                struct.pack_into('2I', shared, 12, *grid)
                struct.pack_into('I', shared, 4, 1)
                print(f'[Looking Glass worker] Ready: {grid[0]}x{grid[1]}', flush=True)
                sequence = frames = 0
                visible = True
                report = time.monotonic()
                while not stopping:
                    glfw.poll_events()
                    active = bool(struct.unpack_from('I', shared, 20)[0])
                    if active != visible:
                        bridge.show_window(panel, active)
                        visible = active
                    current = struct.unpack_from('I', shared, 8)[0]
                    if active and current and not current & 1 and current != sequence:
                        frame = np.frombuffer(shared[64:], dtype=np.uint16).reshape(704, 2048)
                        if current == struct.unpack_from('I', shared, 8)[0]:
                            glfw.make_context_current(window)
                            texture, size = renderer.compose(frame)
                            sequence = current
                            frames += 1
                            if frames == 1:
                                print('[Looking Glass worker] First layer frame received', flush=True)
                    if active and renderer.size:
                        glfw.make_context_current(window)
                        GL.glFlush()
                        bridge.draw_interop_quilt_texture_gl(panel, renderer.quilt,
                            PixelFormats.RGBA, *renderer.size, *grid, 4/3, 1.0)
                    if args.max_frames and frames >= args.max_frames:
                        break
                    now = time.monotonic()
                    if now - report >= 5:
                        print(f'[Looking Glass worker] Received {frames} frames; sequence {sequence}', flush=True)
                        report = now
                    time.sleep(1 / 120)
            except Exception:
                struct.pack_into('I', shared, 4, 2)
                raise
            finally:
                if struct.unpack_from('I', shared, 4)[0] != 2:
                    struct.pack_into('I', shared, 4, 0)
                if window:
                    glfw.make_context_current(window)
                if renderer:
                    renderer.close()
                if bridge:
                    bridge.uninitialize()
                if window:
                    glfw.destroy_window(window)
                glfw.terminate()


if __name__ == '__main__':
    main()
