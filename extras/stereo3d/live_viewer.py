#!/usr/bin/env python3
"""Experimental libretro frontend: GPU quilts, Looking Glass Bridge, SDL2 audio."""
import argparse
import ctypes as C
import ctypes.util
from pathlib import Path
import sys
import time
import glfw
import numpy as np
from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader

VERTEX = """#version 330 core
out vec2 UV;
void main() {
    UV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(UV * 2.0 - 1.0, 0.0, 1.0);
}
"""
PREVIEW = """#version 330 core
uniform sampler2D Quilt;
uniform ivec2 Grid;
in vec2 UV;
out vec4 Color;
void main() {
    int view = Grid.x * Grid.y / 2;
    vec2 tile = vec2(view % Grid.x, Grid.y - 1 - view / Grid.x);
    Color = texture(Quilt, (tile + vec2(UV.x, 1.0 - UV.y)) / vec2(Grid));
}
"""

class Geometry(C.Structure):
    _fields_ = [("width", C.c_uint), ("height", C.c_uint),
                ("max_width", C.c_uint), ("max_height", C.c_uint), ("aspect", C.c_float)]

class Timing(C.Structure):
    _fields_ = [("fps", C.c_double), ("sample_rate", C.c_double)]

class AVInfo(C.Structure):
    _fields_ = [("geometry", Geometry), ("timing", Timing)]

class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p),
                ("size", C.c_size_t), ("meta", C.c_char_p)]

class Variable(C.Structure):
    _fields_ = [("key", C.c_char_p), ("value", C.c_char_p)]

class AudioSpec(C.Structure):
    _fields_ = [("freq", C.c_int), ("format", C.c_uint16), ("channels", C.c_uint8),
                ("silence", C.c_uint8), ("samples", C.c_uint16), ("padding", C.c_uint16),
                ("size", C.c_uint32), ("callback", C.c_void_p), ("userdata", C.c_void_p)]

class Audio:
    def __init__(self, rate, library=None):
        name = library or ctypes.util.find_library("SDL2")
        if not name:
            raise RuntimeError("SDL2 not found; use --sdl /path/to/libSDL2.dylib")
        self.lib = C.CDLL(name)
        self.lib.SDL_InitSubSystem.argtypes = [C.c_uint32]
        self.lib.SDL_InitSubSystem.restype = C.c_int
        self.lib.SDL_GetError.restype = C.c_char_p
        self.lib.SDL_OpenAudioDevice.argtypes = [C.c_char_p, C.c_int,
            C.POINTER(AudioSpec), C.POINTER(AudioSpec), C.c_int]
        self.lib.SDL_OpenAudioDevice.restype = C.c_uint32
        self.lib.SDL_QueueAudio.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]
        self.lib.SDL_QueueAudio.restype = C.c_int
        self.lib.SDL_GetQueuedAudioSize.argtypes = [C.c_uint32]
        self.lib.SDL_GetQueuedAudioSize.restype = C.c_uint32
        self.lib.SDL_PauseAudioDevice.argtypes = [C.c_uint32, C.c_int]
        self.lib.SDL_ClearQueuedAudio.argtypes = [C.c_uint32]
        self.lib.SDL_CloseAudioDevice.argtypes = [C.c_uint32]
        self.lib.SDL_QuitSubSystem.argtypes = [C.c_uint32]
        self.rate = rate
        self.started = False
        if self.lib.SDL_InitSubSystem(0x10):
            raise RuntimeError(self.lib.SDL_GetError().decode())
        want = AudioSpec(freq=rate, format=0x8010, channels=2, samples=1024)
        got = AudioSpec()
        self.device = self.lib.SDL_OpenAudioDevice(None, 0, C.byref(want), C.byref(got), 0)
        if not self.device:
            raise RuntimeError(self.lib.SDL_GetError().decode())
        self.lib.SDL_PauseAudioDevice(self.device, 1)

    def queue(self, data, frames):
        if self.lib.SDL_QueueAudio(self.device, data, frames * 4):
            raise RuntimeError(self.lib.SDL_GetError().decode())
        if not self.started and self.lib.SDL_GetQueuedAudioSize(self.device) >= self.rate * 4 * 0.06:
            self.lib.SDL_PauseAudioDevice(self.device, 0)
            self.started = True

    def pause(self, paused):
        self.lib.SDL_PauseAudioDevice(self.device, 1)
        self.lib.SDL_ClearQueuedAudio(self.device)
        self.started = False

    def close(self):
        self.lib.SDL_CloseAudioDevice(self.device)
        self.lib.SDL_QuitSubSystem(0x10)

class Core:
    def __init__(self, args, window):
        self.lib = C.CDLL(str(args.core.resolve()))
        self.window = window
        self.frame = None
        self.audio = None
        self.errors = []
        self.changed = False
        self.loaded = False
        self.paths = [str(args.save_dir.resolve()).encode()]
        args.save_dir.mkdir(parents=True, exist_ok=True)
        self.options = {
            b"genesis_plus_gx_stereo_3d": b"layers",
            b"genesis_plus_gx_stereo_plane_a": str(args.a).encode(),
            b"genesis_plus_gx_stereo_plane_b": str(args.b).encode(),
            b"genesis_plus_gx_stereo_sprites": str(args.sprites).encode(),
            b"genesis_plus_gx_stereo_swap_eyes": b"enabled" if args.swap else b"disabled",
            b"genesis_plus_gx_frameskip": b"disabled",
        }
        env_t = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
        video_t = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
        batch_t = C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
        sample_t = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
        poll_t = C.CFUNCTYPE(None)
        input_t = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
        self.callbacks = [env_t(self.environment), video_t(self.video), batch_t(self.batch),
                          sample_t(self.sample), poll_t(lambda: None), input_t(self.input)]
        for setter, callback in zip(["environment", "video_refresh", "audio_sample_batch",
                                     "audio_sample", "input_poll", "input_state"], self.callbacks):
            fn = getattr(self.lib, "retro_set_" + setter)
            fn.argtypes = [type(callback)]
            fn.restype = None
            fn(callback)
        for name in ["retro_init", "retro_deinit", "retro_run", "retro_reset", "retro_unload_game"]:
            getattr(self.lib, name).restype = None
            getattr(self.lib, name).argtypes = []
        self.lib.retro_load_game.argtypes = [C.POINTER(GameInfo)]
        self.lib.retro_load_game.restype = C.c_bool
        self.lib.retro_get_system_av_info.argtypes = [C.POINTER(AVInfo)]
        self.lib.retro_get_system_av_info.restype = None
        self.lib.retro_set_controller_port_device.argtypes = [C.c_uint, C.c_uint]
        self.lib.retro_set_controller_port_device.restype = None
        self.lib.retro_init()
        self.rom_path = str(args.rom.resolve()).encode()
        if not self.lib.retro_load_game(C.byref(GameInfo(path=self.rom_path))):
            self.lib.retro_deinit()
            raise RuntimeError("Core could not load the supplied game")
        self.loaded = True
        self.lib.retro_set_controller_port_device(0, 1)
        self.lib.retro_set_controller_port_device(1, 0)
        self.av = AVInfo()
        self.lib.retro_get_system_av_info(C.byref(self.av))

    def environment(self, command, data):
        command &= ~0x10000
        if command in (9, 31):
            C.cast(data, C.POINTER(C.c_char_p))[0] = self.paths[0]
            return True
        if command == 3:
            C.cast(data, C.POINTER(C.c_bool))[0] = True
            return True
        if command == 10:
            return C.cast(data, C.POINTER(C.c_int))[0] == 2
        if command == 15:
            var = C.cast(data, C.POINTER(Variable)).contents
            var.value = self.options.get(var.key)
            return var.value is not None
        if command == 17:
            C.cast(data, C.POINTER(C.c_bool))[0] = self.changed
            self.changed = False
            return True
        if command == 52:
            C.cast(data, C.POINTER(C.c_uint))[0] = 0
            return True
        if command == 47:
            C.cast(data, C.POINTER(C.c_int))[0] = 3
            return True
        if command == 32:
            self.av = AVInfo.from_buffer_copy(C.string_at(data, C.sizeof(AVInfo)))
            return True
        return command in (1, 3, 6, 11, 16, 18, 37)

    def video(self, data, width, height, pitch):
        if not data:
            return
        if (width, height, pitch) != (2048, 704, 4096):
            self.errors.append("Expected GPU layer atlas; check that the new core is loaded")
            return
        self.frame = np.frombuffer(C.string_at(data, pitch * height), dtype=np.uint16).reshape(height, width)

    def batch(self, data, frames):
        try:
            if self.audio: self.audio.queue(data, frames)
        except Exception as error:
            self.errors.append(str(error))
        return frames

    def sample(self, left, right):
        values = (C.c_int16 * 2)(left, right)
        self.batch(values, 1)

    def input(self, port, device, index, button):
        if port or device != 1: return 0
        mapping = {0: glfw.KEY_X, 1: glfw.KEY_Z, 2: glfw.KEY_RIGHT_SHIFT,
                   3: glfw.KEY_ENTER, 4: glfw.KEY_UP, 5: glfw.KEY_DOWN,
                   6: glfw.KEY_LEFT, 7: glfw.KEY_RIGHT, 8: glfw.KEY_C,
                   9: glfw.KEY_A, 10: glfw.KEY_S, 11: glfw.KEY_D}
        key = mapping.get(button)
        return int(key is not None and glfw.get_key(self.window, key) == glfw.PRESS)

    def close(self):
        if self.loaded:
            self.lib.retro_unload_game()
            self.loaded = False
        self.lib.retro_deinit()

class Renderer:
    def __init__(self, grid, aspect=4/3):
        self.grid = grid
        self.aspect = aspect
        self.vao = GL.glGenVertexArrays(1)
        GL.glBindVertexArray(self.vao)
        self.program = compileProgram(compileShader(VERTEX, GL.GL_VERTEX_SHADER),
            compileShader(Path(__file__).with_name("live_quilt.frag").read_text(), GL.GL_FRAGMENT_SHADER))
        self.preview = compileProgram(compileShader(VERTEX, GL.GL_VERTEX_SHADER),
            compileShader(PREVIEW, GL.GL_FRAGMENT_SHADER))
        self.atlas = GL.glGenTextures(1)
        self.quilt = GL.glGenTextures(1)
        self.fbo = GL.glGenFramebuffers(1)
        self.size = None
        self.atlas_ready = False
        GL.glActiveTexture(GL.GL_TEXTURE0)
        for texture in [self.atlas, self.quilt]:
            GL.glBindTexture(GL.GL_TEXTURE_2D, texture)
            GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST)
            GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST)
            GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP_TO_EDGE)
            GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP_TO_EDGE)

    def compose(self, frame):
        width, height = map(int, frame[0, 1664:1666])
        if not 256 <= width <= 348 or not 192 <= height <= 576:
            raise RuntimeError("Invalid layer frame dimensions")
        size = (width * self.grid[0], height * self.grid[1])
        if max(size) > GL.glGetIntegerv(GL.GL_MAX_TEXTURE_SIZE):
            raise RuntimeError("Quilt exceeds GPU texture limit")
        GL.glActiveTexture(GL.GL_TEXTURE0)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.atlas)
        GL.glPixelStorei(GL.GL_UNPACK_ALIGNMENT, 1)
        if not self.atlas_ready:
            GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_R16UI, 2048, 704, 0,
                            GL.GL_RED_INTEGER, GL.GL_UNSIGNED_SHORT, None)
            self.atlas_ready = True
        GL.glTexSubImage2D(GL.GL_TEXTURE_2D, 0, 0, 0, 2048, 704,
                           GL.GL_RED_INTEGER, GL.GL_UNSIGNED_SHORT, frame)
        GL.glBindFramebuffer(GL.GL_FRAMEBUFFER, self.fbo)
        if size != self.size:
            GL.glBindTexture(GL.GL_TEXTURE_2D, self.quilt)
            GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGBA8, *size, 0,
                            GL.GL_RGBA, GL.GL_UNSIGNED_BYTE, None)
            GL.glFramebufferTexture2D(GL.GL_FRAMEBUFFER, GL.GL_COLOR_ATTACHMENT0,
                                      GL.GL_TEXTURE_2D, self.quilt, 0)
            if GL.glCheckFramebufferStatus(GL.GL_FRAMEBUFFER) != GL.GL_FRAMEBUFFER_COMPLETE:
                raise RuntimeError("Quilt framebuffer incomplete")
            self.size = size
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.atlas)
        GL.glViewport(0, 0, *size)
        GL.glUseProgram(self.program)
        GL.glUniform1i(GL.glGetUniformLocation(self.program, "Atlas"), 0)
        GL.glUniform2i(GL.glGetUniformLocation(self.program, "ViewSize"), width, height)
        GL.glUniform2i(GL.glGetUniformLocation(self.program, "Grid"), *self.grid)
        GL.glBindVertexArray(self.vao)
        GL.glDisable(GL.GL_DEPTH_TEST)
        GL.glDisable(GL.GL_BLEND)
        GL.glDisable(GL.GL_SCISSOR_TEST)
        GL.glDrawArrays(GL.GL_TRIANGLES, 0, 3)
        GL.glBindFramebuffer(GL.GL_FRAMEBUFFER, 0)
        return self.quilt, size

    def show_preview(self, window):
        GL.glBindFramebuffer(GL.GL_FRAMEBUFFER, 0)
        width, height = glfw.get_framebuffer_size(window)
        GL.glDisable(GL.GL_SCISSOR_TEST)
        GL.glClearColor(0.0, 0.0, 0.0, 1.0)
        GL.glClear(GL.GL_COLOR_BUFFER_BIT)
        draw_width = min(width, int(height * self.aspect))
        draw_height = min(height, int(width / self.aspect))
        GL.glViewport((width-draw_width)//2, (height-draw_height)//2, draw_width, draw_height)
        GL.glUseProgram(self.preview)
        GL.glActiveTexture(GL.GL_TEXTURE0)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.quilt)
        GL.glUniform1i(GL.glGetUniformLocation(self.preview, "Quilt"), 0)
        GL.glUniform2i(GL.glGetUniformLocation(self.preview, "Grid"), *self.grid)
        GL.glBindVertexArray(self.vao)
        GL.glDrawArrays(GL.GL_TRIANGLES, 0, 3)

    def close(self):
        GL.glDeleteTextures([self.atlas, self.quilt])
        GL.glDeleteFramebuffers(1, [self.fbo])
        GL.glDeleteVertexArrays(1, [self.vao])
        GL.glDeleteProgram(self.program)
        GL.glDeleteProgram(self.preview)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--core", type=Path, default=Path(__file__).parents[2] / "genesis_plus_gx_libretro.dylib")
    parser.add_argument("--save-dir", type=Path, default=Path.home() / "Genesis-LookingGlass")
    parser.add_argument("--sdl", help="SDL2 shared library path")
    parser.add_argument("--a", type=int, default=8)
    parser.add_argument("--b", type=int, default=0)
    parser.add_argument("--sprites", type=int, default=12)
    parser.add_argument("--swap", action="store_true")
    parser.add_argument("--aspect", type=float, default=4/3)
    parser.add_argument("--preview-only", action="store_true", help="Run without Bridge for ordinary-monitor testing")
    args = parser.parse_args()
    if not args.rom.is_file() or not args.core.is_file(): parser.error("ROM and core files must exist")
    if any(not -16 <= v <= 16 for v in (args.a, args.b, args.sprites)): parser.error("depth must be -16..16")
    if not np.isfinite(args.aspect) or args.aspect <= 0: parser.error("aspect must be positive and finite")
    if not glfw.init(): raise RuntimeError("GLFW initialization failed")
    glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 4 if sys.platform == "darwin" else 3)
    glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 1 if sys.platform == "darwin" else 3)
    glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    if sys.platform == "darwin": glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, glfw.TRUE)
    window = glfw.create_window(640, 480, "Genesis Looking Glass: arrows, Z/X/C, Enter; Space pause; F2 swap", None, None)
    if not window: glfw.terminate(); raise RuntimeError("OpenGL window creation failed")
    glfw.make_context_current(window)
    glfw.swap_interval(0)
    bridge = None
    br_window = None
    renderer = core = audio = None
    paused = False
    try:
        grid = (8, 6)
        if not args.preview_only:
            from bridge_python_sdk.BridgeApi import BridgeAPI, PixelFormats
            bridge = BridgeAPI()
            if not bridge.initialize("GenesisLiveQuilt"): raise RuntimeError("Bridge initialization failed")
            br_window = bridge.instance_window_gl(-1)
            _, _, _, columns, rows = bridge.get_default_quilt_settings(br_window)
            grid = (int(columns), int(rows))
            if not 2 <= grid[0]*grid[1] <= 128: raise RuntimeError("Unsupported device quilt grid")
        glfw.make_context_current(window)
        renderer = Renderer(grid, args.aspect)
        core = Core(args, window)
        audio = Audio(int(core.av.timing.sample_rate), args.sdl)
        core.audio = audio
        def key_handler(win, key, scan, action, mods):
            nonlocal paused
            if action != glfw.PRESS: return
            if key == glfw.KEY_ESCAPE: glfw.set_window_should_close(win, True)
            if key == glfw.KEY_SPACE:
                paused = not paused
                audio.pause(paused)
            if key == glfw.KEY_F2:
                k = b"genesis_plus_gx_stereo_swap_eyes"
                core.options[k] = b"disabled" if core.options[k] == b"enabled" else b"enabled"
                core.changed = True
            if key == glfw.KEY_F3:
                core.lib.retro_reset()
                audio.pause(paused)
        glfw.set_key_callback(window, key_handler)
        print(f"GPU live quilt: {grid[0]}x{grid[1]}. Keyboard controls require focus on the preview window.")
        deadline = time.perf_counter()
        report = deadline
        frames = 0
        while not glfw.window_should_close(window):
            glfw.poll_events()
            if not paused:
                core.lib.retro_run()
                if core.errors: raise RuntimeError(core.errors[0])
                if core.frame is None: raise RuntimeError("Core did not produce a layer frame")
                glfw.make_context_current(window)
                texture, size = renderer.compose(core.frame)
            elif renderer.size is None:
                continue
            else:
                texture, size = renderer.quilt, renderer.size
            if bridge:
                GL.glFlush()
                bridge.draw_interop_quilt_texture_gl(br_window, texture, PixelFormats.RGBA,
                    *size, *grid, args.aspect, 1.0)
            glfw.make_context_current(window)
            renderer.show_preview(window)
            glfw.swap_buffers(window)
            frames += int(not paused)
            now = time.perf_counter()
            if now - report >= 5:
                queued = audio.lib.SDL_GetQueuedAudioSize(audio.device) / (audio.rate * 4)
                print(f"{frames/(now-report):.1f} emulated FPS; audio queue {queued*1000:.0f} ms")
                frames = 0
                report = now
            deadline += 1 / core.av.timing.fps
            if deadline < now - 0.1: deadline = now
            time.sleep(max(0, deadline-time.perf_counter()))
    except KeyboardInterrupt:
        pass
    finally:
        if core: core.close()
        if audio: audio.close()
        glfw.make_context_current(window)
        if renderer: renderer.close()
        if bridge: bridge.uninitialize()
        glfw.destroy_window(window)
        glfw.terminate()

if __name__ == "__main__":
    main()
