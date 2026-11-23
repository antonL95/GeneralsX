#!/bin/sh
set -eu

build=$1
out=$2
fixture=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime"
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy DXVK_LOG_PATH="$out"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TEST_COMMANDS="$fixture/cancel.txt"
mkdir -m 700 "$XDG_RUNTIME_DIR"
cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"

timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' \
    env HOME="$out" "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 \
    -noaudio -nomusic -novideo -noshellmap -file 'maps/map mp high pass/map mp high pass.map' \
    -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" > "$out/cancel.log" 2>&1 || {
        tail -25 "$out/cancel.log"
        exit 1
    }
grep -E '^TEST (followup |frame 47: quit)' "$out/cancel.log"
awk '
    /^TEST followup frame=/ {
        frame = substr($3, 7)
        count[frame] = substr($7, 7) + 0
        progress[frame] = substr($8, 10) + 0
        cash[frame] = substr($9, 6) + 0
    }
    END {
        upgrade = count[25] == 1 && progress[25] > 0 && count[30] == 0 && cash[15] - cash[25] == 500 && cash[30] == cash[15]
        unit = count[41] == 1 && progress[41] > 0 && count[46] == 0 && cash[31] - cash[41] == 250 && cash[46] == cash[31]
        printf "Production cancellation and refund: upgrade=%d unit=%d\n", upgrade, unit
        exit !(upgrade && unit)
    }
' "$out/cancel.log"
