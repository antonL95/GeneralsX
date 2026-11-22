#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out=${PKG54_STATE_DIR:-$build/pkg54-fixture}
mkdir -p "$out"
out=$(cd "$out" && pwd)
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_RUNTIME_DIR="$out/runtime"
export SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=x11 LP_NUM_THREADS=3
export GENERALSX_TRACE_RETAIL_FX_LISTS=1 GENERALSX_TEST_COMMANDS="$out/commands.txt"
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export DXVK_LOG_PATH="$out" DXVK_STATE_CACHE_PATH="$out"
name='AotR Training Grounds'
source="$aotr/UserData/Maps/$name"
map="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$name"
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$map"
cp "$source/$name.map" "$source/map.ini" "$source/map.str" "$map/"
printf '\n' >> "$map/map.ini"
cat "$fixtures/pkg54-map.ini" >> "$map/map.ini"
{
    cat "$fixtures/pkg54-commands.txt"
    seq 1 120 | awk '{print $1, "xpcrc - - 0"}'
    printf '%s\n' '54 snapshot-save - - 0 pkg54.sav' '121 quit - - 0'
} | sort -sn > "$GENERALSX_TEST_COMMANDS"

cd "$rotwk"
run_game()
{
    log=$1
    shift
    timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' "$build/GeneralsMD/GeneralsXZH" \
        -win -xres 1280 -yres 720 -noaudio -nomusic -novideo -noshellmap \
        -mod "$aotr" -bfme2 "$bfme2" "$@" > "$out/$log" 2>&1
}

run_game session.log -file "$map/$name.map"
grep '^TEST xp-crc ' "$out/session.log" > "$out/frames.crc"
test "$(wc -l < "$out/frames.crc")" -eq 120
grep -q '^\[Retail FXList\] TerrainScorch cell=[0-3] x=2890.000 y=610.000 radius=8.000$' "$out/session.log"
grep -q '^\[Retail FXList\] TerrainScorch cell=[4-6] x=2900.000 y=620.000 radius=8.000$' "$out/session.log"
grep -q '^\[Retail FXList\] TerrainScorch cell=[4-6] x=2990.000 y=600.000 radius=80.000$' "$out/session.log"
grep -q '^\[Retail FXList\] TerrainScorch cell=[0-3] x=3130.000 y=620.000 radius=150.000$' "$out/session.log"
grep -q '^\[Retail FXList\] TerrainScorch cell=[4-6] x=3140.000 y=620.000 radius=150.000$' "$out/session.log"
test "$(grep -c '^\[Retail FXList\] TerrainScorch ' "$out/session.log")" -eq 5
grep -q '^\[Retail FXList\] BoneFX bone=B_FOOTL weather=0$' "$out/session.log"
grep -q '^\[Retail FXList\] BoneFX bone=B_FOOTL weather=1$' "$out/session.log"
test "$(grep -c '^\[Retail FXList\] BoneFX bone=B_FOOTL ' "$out/session.log")" -eq 2
grep -q '^\[Retail FXList\] ParticleBone name=LightningCharge bone=B_SWORDBONE ' "$out/session.log"
grep -q '^\[Retail FXList\] ParticleBone name=FrodoRingSpark bone=B_HANDL ' "$out/session.log"
grep -q '^\[Retail FXList\] ParticleBone name=ArmoredChargePulseFX bone=ROOTDUMMY ' "$out/session.log"
grep -q '^\[Retail FXList\] ParticleBone name=LightningCharge bone=ROOTDUMMY ' "$out/session.log"
grep -Eq '^TEST particle-endpoint frame=20 name=LightningCharge x=297[0-3]\.' "$out/session.log"
grep -q '^\[Retail FXList\] ParticleOrigin name=LightningStrike3 bone=B_SWORDBONE ' "$out/session.log"
grep -Eq '^TEST particle-endpoint frame=110 name=LightningStrike3 .* z=3[56][0-9]\.' "$out/session.log"
grep -Eq '^TEST particle-endpoint frame=118 name=HSLightningStrike3 x=3150\.000 .* z=3[56][0-9]\.' "$out/session.log"
grep -q '^\[Retail FXList\] DynamicDecal name=exgondortreedecal .* opacity=180 additive=0$' "$out/session.log"
grep -q '^\[Retail FXList\] DynamicDecal expired name=exgondortreedecal ' "$out/session.log"
grep -q '^\[Retail FXList\] DynamicDecal name=SCCCracks .* opacity=255 additive=1$' "$out/session.log"
awk '
/^TEST particle-bone/ && $4 == "name=ArmoredChargePulseFX" {
    if ($3 == "frame=42") before = substr($6, 3) + 0
    if ($3 == "frame=80") after = substr($6, 3) + 0
}
/^\[Retail FXList\] DynamicDecal name=EXScorch03 / {
    frame = substr($5, 7) + 0
    if (frame < 20) early = 1
    if ($6 == "opacity=220") peak = 1
}
/^\[Retail FXList\] DynamicDecal expired name=EXScorch03 / { expired = 1 }
END { exit !(before && after - before > 80 && !early && peak && expired) }
' "$out/session.log"

grep -Fxq 'TEST snapshot-save frame=54 code=0' "$out/session.log"
awk '$1 >= 55 && $2 != "snapshot-save"' "$GENERALSX_TEST_COMMANDS" > "$out/resume.txt"
export GENERALSX_TEST_COMMANDS="$out/resume.txt"
run_game resume.log -loadsave pkg54.sav
grep '^TEST xp-crc ' "$out/resume.log" > "$out/resume.crc"
tail -n 66 "$out/frames.crc" > "$out/continue.crc"
cmp "$out/continue.crc" "$out/resume.crc"
for phase in session resume; do
    awk '/^\[Retail FXList\] DynamicDecal name=exgondortreedecal / && substr($5, 7) + 0 >= 78 ||
        /^\[Retail FXList\] DynamicDecal expired name=exgondortreedecal /' "$out/$phase.log" > "$out/$phase.decal"
    test -s "$out/$phase.decal"
done
cmp "$out/session.decal" "$out/resume.decal"
