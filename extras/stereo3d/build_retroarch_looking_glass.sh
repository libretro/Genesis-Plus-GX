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
cc -DGL_SILENCE_DEPRECATION -std=c89 -Werror=declaration-after-statement -Wall -Wextra -O2 -c \
  "$base/extras/stereo3d/retroarch_looking_glass.c" -o "$build_dir/adapter.o"
cc -DGL_SILENCE_DEPRECATION -std=gnu99 -Wall -Wextra -O2 -c \
  "$base/extras/stereo3d/retroarch_looking_glass_context.m" -o "$build_dir/context.o"
cc -dynamiclib "$build_dir/adapter.o" "$build_dir/context.o" -framework Cocoa -framework OpenGL \
  -o "$base/genesis_looking_glass_libretro.dylib"
