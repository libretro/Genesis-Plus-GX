#!/usr/bin/env python3
"""Run the official quilt example with an explicit macOS OpenGL 4.1 context."""
import runpy
import sys
import glfw

original_init = glfw.init


def init_core_context():
    result = original_init()
    if result and sys.platform == "darwin":
        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 4)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 1)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
        glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, glfw.TRUE)
    return result


glfw.init = init_core_context
runpy.run_module("bridge_python_sdk.Examples.DisplayQuilt", run_name="__main__")
