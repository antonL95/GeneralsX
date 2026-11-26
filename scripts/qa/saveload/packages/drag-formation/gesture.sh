#!/bin/sh
set -eu

build=$1
out=$2
fixture=$(cd "$(dirname "$0")" && pwd)
playtest=$(cd "$fixture/../../../playtest" && pwd)
mkdir -p "$out"
export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime"
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy DXVK_LOG_PATH="$out"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TEST_COMMANDS="$fixture/gesture.txt"
export PLAYTEST_RENDER_FROM=28
export PLAYTEST_FRAMES=35
export PLAYTEST_PNG_35="$out/preview.png"
mkdir -m 700 "$XDG_RUNTIME_DIR"
cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"

timeout --signal=INT --kill-after=10 600 xvfb-run -a -s '-screen 0 1280x720x24' \
    env HOME="$out" gdb -q -nx -batch -return-child-result -x "$playtest/start.gdb" \
    -ex "source $playtest/capture.py" -ex run \
    --args "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 -noFPSLimit \
    -noaudio -nomusic -novideo -noshellmap -file 'maps/map mp high pass/map mp high pass.map' \
    -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" > "$out/gesture.log" 2>&1

grep -E '^TEST drag-(input|selection|preview|formation-state)' "$out/gesture.log"
test -s "$out/preview.png"
awk '
    $1 == "TEST" && $2 == "drag-input" && ($3 == 22 || $3 == 26) && $4 ~ /^-?[0-9]+/ { input++ }
    $1 == "TEST" && $2 == "drag-selection" && ($3 == 23 || $3 == 27) && $4 == 3 { selected++ }
    $1 == "TEST" && $2 == "drag-preview" && $3 == 32 && $4 == 1 && $5 >= 0 && $6 > 0 { previewOn++ }
    $1 == "TEST" && $2 == "drag-preview" && $3 == 37 && $4 == 0 { previewOff++ }
    $1 == "TEST" && $2 == "drag-formation-state" && $3 == 180 {
        n++
        startx = $4 == "GondorArcherHorde" ? 1180 : 1300
        starty = $4 == "GondorArcherHorde" ? 1075 : ($5 == 0 ? 1000 : 1150)
        dx = $6 - startx; dy = $7 - starty
        if (dx * dx + dy * dy < 1600) bad = 1
    }
    END { if (input != 2 || selected != 2 || previewOn != 1 || previewOff != 1 || n != 3 || bad) exit 1 }
' "$out/gesture.log"
