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
export GENERALSX_TEST_COMMANDS="$fixture/recruits.txt" PLAYTEST_FACTION=FactionArnor
mkdir -m 700 "$XDG_RUNTIME_DIR"
cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"

timeout --signal=INT --kill-after=10 180 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -x "$playtest/start.gdb" \
    -ex run --args "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 \
    -noaudio -nomusic -novideo -noshellmap -file 'maps/map mp high pass/map mp high pass.map' \
    -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" > "$out/recruits.log" 2>&1 || {
        tail -25 "$out/recruits.log"
        exit 1
    }
grep -E '^TEST (followup-recruits |frame 45: quit)' "$out/recruits.log"
for line in \
    '14 set=RivendellDunedainEncampmentCommandSet ElvenElladan:0 ElvenElrohir:0' \
    '22 set=RivendellDunedainEncampmentUpgradeCommandSet ElvenElladan:1 ElvenElrohir:1' \
    '27 set=RivendellBreelandCampCommandSet ArnorFattyBolger:0 ArnorFarmerMaggot:0' \
    '33 set=RivendellBreelandCampCommandSetLevel2 ArnorFattyBolger:1 ArnorFarmerMaggot:0' \
    '43 set=RivendellFortressCommandSet ElvenArwen:1 ElvenGildor:1 ArnorHalbarad:1 ElvenAragorn:1 ElvenGlorfindel:1 ElvenElrond:1'; do
    grep -Fxq "TEST followup-recruits frame=$line" "$out/recruits.log" || {
        printf 'Unexpected recruit buttons at frame %s\n' "${line%% *}" >&2
        exit 1
    }
done
