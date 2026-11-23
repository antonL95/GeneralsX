#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    printf 'Usage: %s BUILD_DIR [--measure]\n' "$0" >&2
    exit 2
fi
build=$(cd "$1" && pwd -P)
mode=${2:-}
if [ -n "$mode" ] && [ "$mode" != --measure ]; then
    exit 2
fi
qa=$(cd "$(dirname "$0")" && pwd -P)
out=$(mktemp -d "$build/edge-scroll.XXXXXX")
printf 'Edge scroll logs %s\n' "$out"

export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy LP_NUM_THREADS=2
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export DXVK_LOG_LEVEL=error
cd /game-install/rotwk

run() {
    export EDGE_SCROLL_FPS=$1 EDGE_SCROLL_WIDTH=$2 EDGE_SCROLL_HEIGHT=$3
    export HOME="$out/$1-$2"
    export XDG_RUNTIME_DIR="$HOME/runtime" DXVK_LOG_PATH="$HOME"
    export DXVK_STATE_CACHE_PATH="$HOME"
    export XDG_DATA_HOME="$HOME/data" XDG_CONFIG_HOME="$HOME/config" XDG_CACHE_HOME="$HOME/cache"
    export GENERALSX_TEST_COMMANDS="$HOME/commands.txt"
    userdata="$XDG_DATA_HOME/GeneralsX/GeneralsZH"
    map="$userdata/Maps/aotr training grounds"
    mkdir -p "$map"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    for asset in '/game-install/aotr/UserData/Maps/AotR Training Grounds/'*; do
        name=$(basename "$asset" | tr '[:upper:]' '[:lower:]')
        cp "$asset" "$map/$name"
    done
    printf 'ScrollFactor = 50\nResolution = %s %s\n' "$2" "$3" > "$userdata/Options.ini"
    printf '30 camera Player_1 MenFortressCitadel 0 - 1200 1200 1\n1000 quit - - 0\n' > "$GENERALSX_TEST_COMMANDS"
    log="$HOME/session.log"
    if ! timeout --signal=INT --kill-after=10 180 xvfb-run -a -s "-screen 0 ${2}x${3}x24" \
        gdb -q -nx -batch -return-child-result -x "$qa/playtest/start.gdb" \
        -ex "source $qa/edge-scroll.py" -ex run \
        --args "$build/GeneralsMD/GeneralsXZH" -win -xres "$2" -yres "$3" \
        -noFPSLimit -noaudio -nomusic -novideo -noshellmap -mod /game-install/aotr \
        -bfme2 /game-install/bfme2 -file "$map/aotr training grounds.map" > "$log" 2>&1; then
        tail -35 "$log" >&2
        return 1
    fi
    if ! grep -q '^EDGE_SCROLL_COMPLETE$' "$log" || grep -q 'Python Exception' "$log"; then
        tail -35 "$log" >&2
        return 1
    fi
    grep '^EDGE_SCROLL_RESULT ' "$log" | tee -a "$out/results.txt"
}

run 30 1280 720
run 60 1280 720
run 60 1024 768

if [ "$mode" != --measure ]; then
    awk '
        $4 == "right" && ($5 < 1 || $6 < -5 || $6 > 5) {
            print "Expected rightward movement: " $0
            failed = 1
        }
        $4 == "upper-right" && ($5 < 1 || $6 < 1) {
            print "Expected upper-right movement: " $0
            failed = 1
        }
        !seen[$4]++ {
            x[$4] = $5
            y[$4] = $6
        }
        $5 < x[$4] - 3 || $5 > x[$4] + 3 || $6 < y[$4] - 3 || $6 > y[$4] + 3 {
            print "One-second displacement varies by frame rate or resolution: " $0
            failed = 1
        }
        END {
            if (seen["right"] != 3 || seen["upper-right"] != 3)
                failed = 1
            if (!failed)
                print "EDGE SCROLL PASS"
            exit failed
        }
    ' "$out/results.txt"
fi
