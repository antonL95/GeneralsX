#!/bin/sh
set -eu

build=$1
package=$(cd "$(dirname "$0")" && pwd)
scripts=$(cd "$package/../../.." && pwd)
state=$(mktemp -d "$build/pkg49-animation.XXXXXX")
export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
export SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=x11 LP_NUM_THREADS=3
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
export DXVK_LOG_PATH="$state" DXVK_STATE_CACHE_PATH="$state" DXVK_LOG_LEVEL=error
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TRACE_RETAIL_AUDIO=1 GENERALSX_TEST_COMMANDS="$package/animation.txt"

game_map='/game-install/aotr/UserData/Maps/AotR Training Grounds'
map_dir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/aotr training grounds"
mkdir -p "$map_dir" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
cp "$game_map/AotR Training Grounds.map" "$map_dir/aotr training grounds.map"
cp "$game_map/map.ini" "$game_map/map.str" "$map_dir/"
printf '%s\n' "$state"
cd /game-install/rotwk
timeout --signal=INT --kill-after=10 225 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -x "$scripts/playtest/start.gdb" -ex run --args \
    "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 -noFPSLimit \
    -noaudio -nomusic -novideo -noshellmap -mod /game-install/aotr -bfme2 /game-install/bfme2 \
    -file "$map_dir/aotr training grounds.map" > "$state/run.log" 2>&1

grep -F 'TEST frame 130: quit' "$state/run.log"
awk '
    /^TEST frame 20: create AmrothianGuardsmen / {
        id = $8
        sub(/^id=/, "", id)
        control = id
    }
    /^TEST frame 40: kill object / { dead = $6 }
    /^RETAIL AUDIO start frame=/ && / sound=BodyFallGeneric1$/ {
        owner = $5
        sub(/^owner=/, "", owner)
        if (dead != "" && owner == dead) ++cues
        if (control != "" && owner == control) ++controlCues
    }
    END {
        if (dead == "" || control == "" || control == dead || cues != 1 || controlCues != 0)
            exit 1
        printf "Animation audio matched for object %s with silent control %s\n", dead, control
    }
' "$state/run.log"
