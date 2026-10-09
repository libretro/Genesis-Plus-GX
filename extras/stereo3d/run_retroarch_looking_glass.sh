#!/bin/sh
set -eu
file_limit=$(ulimit -S -n)
if [ "$file_limit" != unlimited ] && [ "$file_limit" -lt 4096 ]; then
  if ! ulimit -S -n 4096; then
    echo 'Cannot raise the open-file limit to 4096 for RetroArch.' >&2
    exit 1
  fi
fi
base=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
export GENESIS_LOOKING_GLASS_CORE="$base/genesis_plus_gx_libretro.dylib"
export GENESIS_LOOKING_GLASS_SHADER="$base/extras/stereo3d/live_quilt.frag"
export GENESIS_LOOKING_GLASS_BRIDGE=${GENESIS_LOOKING_GLASS_BRIDGE:-'/Applications/Looking Glass Bridge 2.6.3.app/Contents/MacOS/libbridge_inproc.dylib'}
retroarch=${RETROARCH_BIN:-'/Applications/RetroArch.app/Contents/MacOS/RetroArch'}
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 /absolute/path/to/game.md [RetroArch arguments]" >&2
  exit 1
fi
if [ ! -f "$base/genesis_looking_glass_libretro.dylib" ]; then
  echo 'Run build_retroarch_looking_glass.sh first.' >&2
  exit 1
fi
if [ ! -f "$GENESIS_LOOKING_GLASS_BRIDGE" ]; then
  echo 'Set GENESIS_LOOKING_GLASS_BRIDGE to the installed Bridge library.' >&2
  exit 1
fi
config_dir="$base/extras/stereo3d/.retroarch-live"
mkdir -p "$config_dir"
if [ ! -f "$config_dir/core-options.cfg" ]; then
  cat > "$config_dir/core-options.cfg" <<'OPTIONS'
genesis_plus_gx_stereo_3d = "enabled"
genesis_plus_gx_stereo_plane_a = "8"
genesis_plus_gx_stereo_plane_b = "0"
genesis_plus_gx_stereo_sprites = "12"
genesis_plus_gx_stereo_swap_eyes = "enabled"
OPTIONS
fi
base_config=${RETROARCH_CONFIG:-"$HOME/Library/Application Support/RetroArch/config/retroarch.cfg"}
if [ -f "$base_config" ]; then
  cp "$base_config" "$config_dir/session.cfg"
else
  : > "$config_dir/session.cfg"
fi
cat > "$config_dir/retroarch.cfg" <<CONFIG
config_save_on_exit = "false"
global_core_options = "true"
core_options_path = "$config_dir/core-options.cfg"
video_fullscreen = "false"
video_windowed_fullscreen = "false"
video_shader_enable = "false"
video_filter = ""
CONFIG
python=${GENESIS_LOOKING_GLASS_PYTHON:-"$HOME/looking-glass-venv/bin/python"}
if [ ! -x "$python" ]; then
  echo 'Set GENESIS_LOOKING_GLASS_PYTHON to Python with the live viewer dependencies.' >&2
  exit 1
fi
export GENESIS_LOOKING_GLASS_STREAM=$(mktemp "$config_dir/atlas.XXXXXX")
worker_pid=
cleanup() {
  if [ -n "$worker_pid" ]; then
    kill "$worker_pid" 2>/dev/null || true
    wait "$worker_pid" 2>/dev/null || true
  fi
  rm -f "$GENESIS_LOOKING_GLASS_STREAM"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"$python" "$base/extras/stereo3d/retroarch_bridge_worker.py" "$GENESIS_LOOKING_GLASS_STREAM" > "$config_dir/bridge.log" 2>&1 &
worker_pid=$!
if ! "$python" - "$GENESIS_LOOKING_GLASS_STREAM" <<'READY'
import pathlib, struct, sys, time
path = pathlib.Path(sys.argv[1])
for _ in range(200):
    with path.open('rb') as file:
        header = file.read(8)
    if len(header) == 8:
        magic, ready = struct.unpack('2I', header)
        if magic == 0x474c4741 and ready == 1:
            sys.exit(0)
        if magic == 0x474c4741 and ready == 2:
            sys.exit(1)
    time.sleep(0.1)
sys.exit(1)
READY
then
  cat "$config_dir/bridge.log" >&2
  echo 'Bridge worker did not initialize; RetroArch was not started.' >&2
  exit 1
fi
"$retroarch" --verbose --log-file "$config_dir/retroarch.log" --config "$config_dir/session.cfg" --appendconfig "$config_dir/retroarch.cfg" \
  -L "$base/genesis_looking_glass_libretro.dylib" "$@"
