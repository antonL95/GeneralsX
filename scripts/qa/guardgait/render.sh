set -eu
build=${1:?build directory required}
label=${2:?capture label required}
fixture=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
state="$build/guardgait-state"
out="$state/$label"
export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
export DXVK_LOG_PATH="$state" DXVK_STATE_CACHE_PATH="$state"
mkdir -p -m 700 "$XDG_RUNTIME_DIR" "$XDG_CACHE_HOME" "$out"
export GENERALSX_TEST_COMMANDS="$out/commands.txt"
{
    cat "$fixture/reform.txt"
    printf '%s\n' '25 create Player_1 ArnorCavalry 0 2430,795,0' '65 builderscriptmove Player_1 ArnorCavalry 0 - 2615 795'
} | sort -sn > "$GENERALSX_TEST_COMMANDS"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy LP_NUM_THREADS=3
mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/aotr training grounds"
package='/game-install/aotr/UserData/Maps/AotR Training Grounds'
mkdir -p "$mapdir"
map="$mapdir/aotr training grounds.map"
cp "$package/AotR Training Grounds.map" "$map"
cp "$package/map.ini" "$package/map.str" "$mapdir/"
sed '/^break PartitionManager/,$d' /app/tools/testmap_verify.gdb > "$out/start.gdb"
frames="96 108 116 124 132 140 148 156 202 220 $(seq 224 2 280) 296 312 322"
export GUARDGAIT_SHOTS=$(printf '%s\n' $frames | paste -sd ,)
for frame in $frames; do
    export "GUARDGAIT_PNG_$frame=$out/frame-$frame.png"
done
cd /game-install/rotwk
timeout --signal=INT --kill-after=10 420 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -x "$out/start.gdb" \
    -ex "source $fixture/capture.py" -ex run \
    --args "${GUARDGAIT_BINARY:-$build/GeneralsMD/GeneralsXZH}" -win -xres 1280 -yres 720 -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
    -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$map" > "$out/session.log" 2>&1 ||
    { tail -45 "$out/session.log"; exit 1; }
if grep -E 'Traceback|Python Exception|No symbol|not defined' "$out/session.log"; then
    exit 1
fi
for frame in $frames; do
    test -s "$out/frame-$frame.png"
done
grep '^GAIT ' "$out/session.log" > "$out/gait.tsv"
wc -l "$out/gait.tsv"
