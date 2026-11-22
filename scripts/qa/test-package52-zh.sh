#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out=$(mktemp -d "$build/pkg52.XXXXXX")
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy GENERALSX_TRACE_RETAIL_FX_LISTS=1
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export VK_DRIVER_FILES=$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)
export SDL_VIDEO_DRIVER=x11 LP_NUM_THREADS=3 DXVK_LOG_PATH="$out" DXVK_STATE_CACHE_PATH="$out"
export GENERALSX_TEST_COMMANDS="$out/commands.txt"
name='AotR Training Grounds'
source="$aotr/UserData/Maps/$name"
map="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$name"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$map"
cp "$source/$name.map" "$source/map.ini" "$source/map.str" "$map/"
printf '\n' >> "$map/map.ini"
cat "$fixtures/pkg52-map.ini" >> "$map/map.ini"
{
    cat "$fixtures/pkg52-commands.txt"
    seq 1 193 | awk '{print $1, "xpcrc - - 0"}'
    printf '%s\n' '194 quit - - 0'
} | sort -sn > "$GENERALSX_TEST_COMMANDS"

cd "$rotwk"
timeout --signal=INT --kill-after=10 240 xvfb-run -a -s '-screen 0 1280x720x24' "$build/GeneralsMD/GeneralsXZH" \
    -win -xres 1280 -yres 720 -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
    -mod "$aotr" -bfme2 "$bfme2" -file "$map/$name.map" > "$out/session.log" 2>&1
grep -E '^TEST (particle-|xp-crc|pkg50 frame=(115|129))|^\[Retail FXList\] (Sound (Hero|UrukDieFalling)|EvaEvent (LocalPlayerLosesRing|AragornDie))' "$out/session.log" > "$out/results.log"
grep '^TEST xp-crc ' "$out/results.log" > "$out/frames.crc"
test "$(wc -l < "$out/frames.crc")" -eq 193
grep -v '^TEST xp-crc ' "$out/results.log"
test "$(grep -c '^\[Retail FXList\] Sound Hero' "$out/results.log")" -eq 1
grep -q '^\[Retail FXList\] Sound HeroWestFemaleVoiceDie$' "$out/results.log"
test "$(grep -c '^\[Retail FXList\] Sound UrukDieFalling$' "$out/results.log")" -eq 1
test "$(grep -c '^\[Retail FXList\] EvaEvent LocalPlayerLosesRing$' "$out/results.log")" -eq 1
test "$(grep -c '^\[Retail FXList\] EvaEvent AragornDie$' "$out/results.log")" -eq 1
grep -Eq '^TEST particle-systems frame=21 name=GandalfIstariLightRays .*targeted=1 ' "$out/results.log"
grep -q '^TEST particle-systems frame=23 name=CatapultExplosionSmoke systems=1 ' "$out/results.log"
grep -q '^TEST particle-systems frame=23 name=ProjectileSplash01 systems=0 ' "$out/results.log"
grep -q '^TEST particle-water-fx frame=24 ' "$out/results.log"
grep -q '^TEST particle-systems frame=25 name=CatapultExplosionSmoke systems=1 ' "$out/results.log"
grep -q '^TEST particle-systems frame=25 name=ProjectileSplash01 systems=1 ' "$out/results.log"
grep -q '^TEST particle-endpoint frame=28 name=HSLightningStrike3 x=2975.000 y=485.000 z=360.000$' "$out/results.log"
test "$(grep -c '^TEST particle-endpoint frame=29 name=HSLightningStrike ' "$out/results.log")" -eq 4
grep -q '^TEST particle-systems frame=29 name=HSLightningStrike systems=5 ' "$out/results.log"
grep -Eq '^TEST particle-systems frame=34 name=AODLightShafts systems=[5-8] ' "$out/results.log"
grep -Eq '^TEST particle-systems frame=38 name=AODLightLenzflare systems=[6-9] ' "$out/results.log"
grep -q '^TEST particle-systems frame=110 name=AODLightLenzflare systems=0 ' "$out/results.log"
grep -Eq '^TEST pkg50 frame=115 name=Pkg52QuietSanctum present=1 .*ability=10000 ' "$out/results.log"
grep -Eq '^TEST pkg50 frame=129 name=SanctumofZigur present=1 .*ability=10000 ' "$out/results.log"
awk '$2 == "particle-systems" && $4 == "name=HSLightningStrike3" {
    split($3, frame, "="); split($5, systems, "="); count[frame[2]] = systems[2]
} END {
    exit !((111 in count) && (115 in count) && (129 in count) && count[111] == count[115] && count[129] == count[115] + 4)
}' "$out/results.log"
grep -q '^TEST particle-systems frame=186 name=AODLightShafts systems=1 ' "$out/results.log"
grep -q '^TEST particle-systems frame=190 name=GPUUnitFire systems=0 ' "$out/results.log"
grep -q '^TEST particle-systems frame=192 name=GPUUnitFire systems=1 ' "$out/results.log"
