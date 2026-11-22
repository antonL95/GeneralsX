#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out="$build/spawn-fixture"
mkdir -p -m 700 "$out/runtime"
printf '%s\n' "$out"
export XDG_DATA_HOME="$out/data" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"

{
    cat "$fixtures/retail-spawn-points-commands.txt"
    seq 1 180 | awk '{print $1, "xpcrc - - 0"}'
} | sort -sn > "$out/commands.txt"
awk '$1 >= 19 && $2 != "snapshot-save"' "$out/commands.txt" > "$out/early.txt"
awk '$1 >= 101' "$out/commands.txt" > "$out/resume.txt"

cd "$rotwk"
run_game()
{
    phase=$1
    commands=$2
    shift 2
    export GENERALSX_TEST_COMMANDS="$out/$commands.txt"
    timeout --signal=INT --kill-after=10 120 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit \
        -noaudio -nomusic -novideo -noshellmap -mod "$aotr" -bfme2 "$bfme2" "$@" > "$out/$phase.log" 2>&1
    grep '^TEST xp-crc ' "$out/$phase.log" > "$out/$phase.crc"
}

map="$aotr/UserData/Maps/AotR Training Grounds/AotR Training Grounds.map"
run_game first commands -file "$map"
run_game second commands -file "$map"
test "$(wc -l < "$out/first.crc")" -eq 180
cmp "$out/first.crc" "$out/second.crc"
for phase in first second; do
    log="$out/$phase.log"
    grep -Fx 'TEST spawn-points frame=18 prefix=ARROW_ bones=4 raised=0 occupants=0 matched=0' "$log"
    grep -Fx 'TEST snapshot-save frame=18 code=0' "$log"
    grep -Eq '^PKG03 frame=90 list=trebuchet type=GondorBigTrebuchet_Summoned .* pos=30[0-9][0-9]\.' "$log"
    grep -Eq '^PKG03 frame=90 list=guards type=GondorBigTrebuchetGuards .* pos=30[0-9][0-9]\.' "$log"
    grep -Eq '^TEST spawn-points frame=90 prefix=BOYO bones=12 raised=[0-9]+ occupants=12 matched=12$' "$log"
    grep -Fx 'TEST snapshot-save frame=100 code=0' "$log"
done

run_game early early -loadsave barricade-early.sav
tail -n 162 "$out/second.crc" > "$out/early-continued.crc"
cmp "$out/early-continued.crc" "$out/early.crc"
run_game resume resume -loadsave spawn-points.sav
tail -n 80 "$out/second.crc" > "$out/continued.crc"
cmp "$out/continued.crc" "$out/resume.crc"
for phase in first second early resume; do
    grep -Fx 'TEST spawn-points frame=180 prefix=ARROW_ bones=4 raised=4 occupants=4 matched=4' "$out/$phase.log"
    grep -Eq '^TEST spawn-points frame=180 prefix=BOYO bones=12 raised=[0-9]+ occupants=12 matched=12$' "$out/$phase.log"
    grep -Eq '^TEST spawn-points frame=180 prefix=ARCHER_ bones=4 raised=[0-9]+ occupants=4 matched=4$' "$out/$phase.log"
done
