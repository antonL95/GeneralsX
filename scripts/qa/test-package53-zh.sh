#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out=$(mktemp -d "$build/pkg53.XXXXXX")
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_RUNTIME_DIR="$out/runtime"
export SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=x11 LP_NUM_THREADS=3
export GENERALSX_TRACE_RETAIL_FX_LISTS=1 GENERALSX_TEST_DRAWTRACE=TDHDwarvesCampDoor,IsengardOrthancDoor,Pkg53StaticGate
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export DXVK_LOG_PATH="$out" DXVK_STATE_CACHE_PATH="$out"
export GENERALSX_TEST_COMMANDS="$out/commands.txt"
name='AotR Training Grounds'
source="$aotr/UserData/Maps/$name"
map="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$name"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$map"
cp "$source/$name.map" "$source/map.ini" "$source/map.str" "$map/"
printf '\n' >> "$map/map.ini"
cat "$fixtures/pkg53-map.ini" >> "$map/map.ini"
{
    cat "$fixtures/pkg53-commands.txt"
    seq 1 104 | awk '{print $1, "xpcrc - - 0"}'
    printf '%s\n' '105 quit - - 0'
} | sort -sn > "$GENERALSX_TEST_COMMANDS"

cd "$rotwk"
timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' "$build/GeneralsMD/GeneralsXZH" \
    -win -xres 1280 -yres 720 -noaudio -nomusic -novideo -noshellmap \
    -mod "$aotr" -bfme2 "$bfme2" -file "$map/$name.map" > "$out/session.log" 2>&1
grep '^TEST xp-crc ' "$out/session.log" > "$out/frames.crc"
test "$(wc -l < "$out/frames.crc")" -eq 104
test "$(grep -c '^\[Retail FXList\] ViewShake ' "$out/session.log")" -eq 1
test "$(grep -c '^\[Retail FXList\] TintDrawable attack=100 hold=400 decay=100 rgb=(1.000,1.000,1.000)$' "$out/session.log")" -eq 1
test "$(grep -c '^\[Retail FXList\] TintDrawable .* rgb=(-0.376,-0.376,-0.376)$' "$out/session.log")" -eq 1
test "$(grep -c '^\[Retail FXList\] CameraShakerVolume radius=500.000 duration=0.500 amplitude=0.500$' "$out/session.log")" -eq 1
grep -Eq '^TEST damage .* weapon=EaglePlowAttack .* victim=[0-9]+\(IsengardFighter\).* dealt=[1-9]' "$out/session.log"
grep -Eq '^TEST damage .* weapon=EaglePlowAttack .* victim=[0-9]+\(MenFortressCitadel\).* dealt=[1-9]' "$out/session.log"
awk '
function nearColor(color, expected,   channel, i) {
    split(substr(color, 6), channel, ",")
    for (i = 1; i <= 3; ++i)
        if ((channel[i] - expected)^2 > 0.0001)
            return 0
    return 1
}
/^TEST pkg53 tint/ && $5 == "disabled=1" {
    if ($4 == "frame=60") {
        gray = substr($6, 6) + 0
        beforeTint = gray < -0.45 && nearColor($6, gray)
    }
    if ($4 == "frame=65") duringTint = nearColor($6, gray + 1)
    if ($4 == "frame=100") afterTint = nearColor($6, gray)
}
/^TEST pkg53 flyer-hit .*structure=0$/ { hit = "unit" }
/^TEST pkg53 flyer-hit .*structure=1$/ { hit = "structure" }
/^\[Retail FXList\] ViewShake type=2$/ { shaken = hit }
/^TEST draw frame/ && $7 == "Pkg53StaticGate" && $4 >= 6 && $4 < 9 {
    if (/flags  subobjects/ && /IBGATEDOORR:hidden/ && /IBGATEDOORR_D1:shown/)
        staticGate = 1
}
/^TEST draw frame/ && $7 == "IsengardOrthancDoor" {
    if ($4 >= 6 && $4 < 18 && /IBGATEDOORR:shown/ && /IBGATEDOORR_D1:hidden/ && /IBGATEDOORR_D2:hidden/) pristine = 1
    if ($4 >= 18 && $4 < 27 && /IBGATEDOORR:hidden/ && /IBGATEDOORR_D1:shown/ && /IBGATEDOORR_D2:hidden/) damaged = 1
    if ($4 >= 27 && $4 < 36 && /IBGATEDOORR:hidden/ && /IBGATEDOORR_D1:hidden/ && /IBGATEDOORR_D2:shown/) severe = 1
    if ($4 >= 36 && /IBGATEDOORR:shown/ && /IBGATEDOORR_D1:hidden/ && /IBGATEDOORR_D2:hidden/) repaired = 1
}
END { exit !(pristine && damaged && severe && repaired && staticGate && shaken == "structure" && beforeTint && duringTint && afterTint) }
' "$out/session.log"
