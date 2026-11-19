#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${GENESIS_LOOKING_GLASS_SDK_HEADERS:?Set the external BridgeRuntime header directory}"
python=${GENESIS_LOOKING_GLASS_PYTHON:-python3}
build=$(mktemp -d "${TMPDIR:-/tmp}/genesis-metal-test.XXXXXX")
trap 'rm -rf "$build"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
clang++ -DGL_SILENCE_DEPRECATION -std=c++17 -fobjc-arc -Wall -Wextra -O2 \
  -I"$GENESIS_LOOKING_GLASS_SDK_HEADERS" -dynamiclib \
  "$base/retroarch_looking_glass_metal.mm" -framework Cocoa -framework OpenGL \
  -framework Metal -o "$build/adapter-metal.dylib"
clang++ -std=c++17 -fobjc-arc -Wall -Wextra -O2 \
  -I"$GENESIS_LOOKING_GLASS_SDK_HEADERS" -dynamiclib \
  "$base/tests/metal_fixture.mm" -framework Foundation -framework Metal \
  -framework IOSurface -o "$build/metal_fixture.dylib"
unset GENESIS_LOOKING_GLASS_MAX_FPS
"$python" "$base/tests/test_metal_compositor.py" "$build"
