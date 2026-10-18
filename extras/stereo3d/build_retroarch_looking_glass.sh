#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case $(uname -s) in
  Darwin) ;;
  *) echo 'This experimental adapter currently requires macOS.' >&2; exit 1 ;;
esac
make -C "$base" -f Makefile.libretro -j4
build_dir="$base/extras/stereo3d/.retroarch-live"
mkdir -p "$build_dir"
metal_cflags=
metal_objects=
metal_ldflags=
sdk_headers=${GENESIS_LOOKING_GLASS_SDK_HEADERS:-}
if [ -n "$sdk_headers" ]; then
  if [ ! -f "$sdk_headers/bridge.h" ]; then
    echo 'GENESIS_LOOKING_GLASS_SDK_HEADERS must contain bridge.h.' >&2
    exit 1
  fi
  clang++ -DGL_SILENCE_DEPRECATION -std=c++17 -fobjc-arc -Wall -Wextra -O2 \
    -I"$sdk_headers" -c "$base/extras/stereo3d/retroarch_looking_glass_metal.mm" \
    -o "$build_dir/metal.o"
  metal_cflags=-DHAVE_LOOKING_GLASS_METAL
  metal_objects="$build_dir/metal.o"
  metal_ldflags='-framework Metal -lc++'
fi
cc -DGL_SILENCE_DEPRECATION $metal_cflags -std=c89 -Werror=declaration-after-statement -Wall -Wextra -O2 -c \
  "$base/extras/stereo3d/retroarch_looking_glass.c" -o "$build_dir/adapter.o"
cc -DGL_SILENCE_DEPRECATION -std=gnu99 -Wall -Wextra -O2 -c \
  "$base/extras/stereo3d/retroarch_looking_glass_context.m" -o "$build_dir/context.o"
if [ -n "$metal_objects" ]; then
  cc -dynamiclib "$build_dir/adapter.o" "$build_dir/context.o" "$metal_objects" \
    -framework Cocoa -framework OpenGL $metal_ldflags \
    -o "$base/genesis_looking_glass_libretro.dylib"
else
  cc -dynamiclib "$build_dir/adapter.o" "$build_dir/context.o" \
    -framework Cocoa -framework OpenGL -o "$base/genesis_looking_glass_libretro.dylib"
fi
