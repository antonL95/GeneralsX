#!/bin/sh
set -eu

build=$1
out=$2
fixture=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
export HOME="$out"
export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime"
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy DXVK_LOG_PATH="$out"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TEST_COMMANDS="$fixture/replay.txt"
mkdir -m 700 "$XDG_RUNTIME_DIR"
cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"
engine="$build/GeneralsMD/GeneralsXZH"
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
map_dir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/map mp high pass"
mkdir -p "$map_dir"
cp "$aotr/maps/map mp high pass/map mp high pass.map" "$map_dir/"
cp "$aotr/maps/map mp high pass/map.ini" "$map_dir/"
set -- -mod "$aotr" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" \
    -noFPSLimit -noaudio -nomusic -novideo -noshellmap
if ! timeout --signal=INT --kill-after=10 120 \
    gdb -q -nx -batch -return-child-result -ex 'set debuginfod enabled off' \
    -ex 'set print thread-events off' -x "$fixture/replay-live.gdb" -ex run --args "$engine" \
    -headless -seed 1 -file "$map_dir/map mp high pass.map" "$@" > "$out/live.log" 2>&1; then
    tail -25 "$out/live.log"
    exit 1
fi
replay=$(find "$XDG_DATA_HOME" -type f -iname '*.rep' -print -quit)
test -n "$replay"
LC_ALL=C grep -aFq ';SC=1500;' "$replay"

timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -ex 'set debuginfod enabled off' \
    -ex 'set print thread-events off' -x "$fixture/replay-playback.gdb" -ex run --args "$engine" \
    -win -xres 1280 -yres 720 -loadreplay "$replay" "$@" > "$out/playback.log" 2>&1 || {
        tail -25 "$out/playback.log"
        exit 1
    }
grep -Fx 'TEST pkg23 cash frame=1 label=replay-start amount=1500' "$out/live.log"
grep -Fx 'TEST pkg23 cash frame=1 label=replay-start amount=1500' "$out/playback.log"
grep -E '^TEST followup replay cleared at frame [0-9]+$' "$out/playback.log"
grep -Fx 'TEST followup replay return menu drew and received Escape' "$out/playback.log"
grep -E '^\[Inferior 1 .* exited normally\]$' "$out/playback.log"
