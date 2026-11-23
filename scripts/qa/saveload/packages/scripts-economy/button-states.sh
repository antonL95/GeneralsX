#!/bin/sh
set -eu

build=$1
out=$2
fixture=$(cd "$(dirname "$0")" && pwd)
playtest=$(cd "$fixture/../../../playtest" && pwd)
mkdir -p "$out"
export HOME="$out"
export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime"
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy DXVK_LOG_PATH="$out"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TEST_COMMANDS="$fixture/button-states.txt"
export PLAYTEST_FRAMES='14 17 36 43 50' PLAYTEST_RENDER_FROM=0
for frame in $PLAYTEST_FRAMES; do
    export "PLAYTEST_PNG_${frame}=$out/frame-$frame.png"
done
mkdir -m 700 "$XDG_RUNTIME_DIR"
cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"

timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -x "$playtest/start.gdb" -ex "source $playtest/capture.py" \
    -ex run --args "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 \
    -noaudio -nomusic -novideo -noshellmap -file 'maps/map mp high pass/map mp high pass.map' \
    -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" > "$out/buttons.log" 2>&1 || {
        tail -25 "$out/buttons.log"
        exit 1
    }
grep -E '^TEST (followup |frame 53: quit)' "$out/buttons.log"
/opt/playtest-media/bin/python -I "$fixture/button-states.py" "$out"
