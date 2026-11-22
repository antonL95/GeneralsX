#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out="$build/buff-fixture"
mkdir -p -m 700 "$out/runtime"
printf '%s\n' "$out"
export XDG_DATA_HOME="$out/data" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=x11 LP_NUM_THREADS=3
export GENERALSX_TRACE_RETAIL_FX_LISTS=1 GENERALSX_TEST_COMMANDS="$out/verified.txt"
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export DXVK_LOG_PATH="$out" DXVK_STATE_CACHE_PATH="$out"
user_data="$XDG_DATA_HOME/GeneralsX/GeneralsZH"
mkdir -p "$user_data"
printf 'RetailStaticGameLOD = UltraHigh\nRetailIdealStaticGameLOD = UltraHigh\nResolution = 1280 720\n' > "$user_data/Options.ini"

{
    cat "$fixtures/retail-buff-commands.txt"
    seq 1 159 | awk '{print $1, "xpcrc - - 0"}'
} | sort -sn > "$GENERALSX_TEST_COMMANDS"

cd "$rotwk"
for mode in render headless; do
    set -- "$build/GeneralsMD/GeneralsXZH" -noaudio -nomusic -novideo -noshellmap \
        -mod "$aotr" -bfme2 "$bfme2" -file "$aotr/UserData/Maps/AotR Training Grounds/AotR Training Grounds.map"
    if [ "$mode" = render ]; then
        set -- xvfb-run -a -s '-screen 0 1280x720x24' "$@" -win -xres 1280 -yres 720
    else
        set -- "$@" -headless -noFPSLimit
    fi
    timeout --signal=INT --kill-after=10 220 "$@" > "$out/$mode.log" 2>&1
    grep '^TEST xp-crc ' "$out/$mode.log" > "$out/$mode.crc"
    test "$(wc -l < "$out/$mode.crc")" -eq 159
done
cmp "$out/render.crc" "$out/headless.crc"

log="$out/render.log"
grep -Eq '^\[Retail Buff\] model=FXLdrship1E owner=GondorTowerShieldGuard .* duration=90 loaded=1 preDraw=1$' "$log"
grep -q '^\[Retail Buff\] refresh model=FXLdrship1E owner=GondorTowerShieldGuard ' "$log"
grep -Eq '^\[Retail Buff\] model=EXHealbuff owner=GondorTowerShieldGuard .* duration=72 loaded=1 preDraw=0$' "$log"
grep -q '^\[Retail Buff\] expired model=EXHealbuff owner=GondorTowerShieldGuard ' "$log"
grep -Eq '^TEST particle-systems frame=34 name=AragornAthelas systems=1 particles=[1-9]' "$log"
grep -Eq '^TEST particle-systems frame=34 name=AragornAthelas02 systems=1 particles=[1-9]' "$log"
awk '
    /^TEST health (11|35) Player_1 GondorElessar / {
        if (substr($8, 8) + 0 == substr($9, 5) + 0) ++full
    }
    /^TEST health 66 Player_1 GondorTowerShieldGuard / { wounded = substr($8, 8) + 0 }
    /^TEST health 78 Player_1 GondorTowerShieldGuard / { healed = substr($8, 8) + 0 }
    END { exit !(full == 2 && wounded > 0 && healed > wounded) }
' "$log"
