#!/bin/sh
set -eu
export LC_ALL=C

build=$1
out=$2
phase=$3
case "$phase" in base|resume) ;; *) exit 2 ;; esac

package=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
fixtures=$(CDPATH= cd -- "$package/../.." && pwd)
export XDG_DATA_HOME="$out/$phase/data" XDG_CONFIG_HOME="$out/$phase/config"
export XDG_CACHE_HOME="$out/$phase/cache" XDG_RUNTIME_DIR="$out/$phase/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
gameData="$XDG_DATA_HOME/GeneralsX/GeneralsZH"
mapName='map mp high pass'
map="$gameData/Maps/$mapName/$mapName.map"
aotr=/game-install/aotr
mkdir -p "$gameData/Save" "$(dirname "$map")"
cp "$aotr/maps/$mapName/$mapName.map" "$map"
cat "$aotr/maps/$mapName/map.ini" "$package/legacy-dry.ini" > "$(dirname "$map")/map.ini"

if [ "$phase" = base ]; then
    cp "$fixtures/training.gdb" "$out/$phase.gdb"
    cp "$package/legacy-dry.txt" "$out/$phase.txt"
    set -- -file "$map"
else
    cp "$out/legacy-dry.sav" "$gameData/Save/"
    sed '/^break ScriptEngine::startEndGameTimer/,$!d' "$fixtures/training.gdb" > "$out/$phase.gdb"
    awk '$1 > 351' "$package/legacy-dry.txt" > "$out/$phase.txt"
    set -- -loadsave legacy-dry.sav
fi

{
    seq 352 655 | awk '{ print $1, "xpcrc - - 0" }'
    printf '655 quit - - 0\n'
} >> "$out/$phase.txt"
sort -s -n -k1,1 "$out/$phase.txt" -o "$out/$phase.txt"

export GENERALSX_TEST_COMMANDS="$out/$phase.txt"
cd /game-install/rotwk
timeout --signal=INT --kill-after=10 450 env -u HOME gdb -q -nx -batch -return-child-result \
    -x "$out/$phase.gdb" -ex run --args "${DEEP_WATER_BINARY:-"$build/GeneralsMD/GeneralsXZH"}" \
    -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap -mod "$aotr" \
    -bfme2 /game-install/bfme2 "$@" > "$out/$phase.log" 2>&1 || {
    tail -40 "$out/$phase.log" >&2
    exit 1
}
grep -Fx 'TEST frame 655: quit' "$out/$phase.log"
awk '
    /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
        split($3, f, "=")
        if (f[2] != 352 + count++ || length($4) != 12) exit 1
        print f[2], $4
    }
    END { if (count != 304) exit 1 }
' "$out/$phase.log" > "$out/$phase.crc"

if [ "$phase" = base ]; then
    grep -Fx 'TEST snapshot-save frame=351 code=0' "$out/$phase.log"
    grep -Fx 'TEST deep-water-route 20 DeepWaterDryRubbleProbe exists=1 src=0 dst=3 goal=2255.0,1095.0' "$out/$phase.log"
    grep -E '^TEST deep-water-sample 85 DeepWaterDryRubbleProbe .* cell=3 ' "$out/$phase.log"
    grep -Fx 'TEST deep-water-route 348 DeepWaterDryLandProbe exists=1 src=0 dst=0 goal=2255.0,1005.0' "$out/$phase.log"
    grep -Fx 'TEST deep-water-route 350 DeepWaterDryLandProbe exists=1 src=0 dst=0 goal=2255.0,1005.0' "$out/$phase.log"
    cp "$gameData/Save/legacy-dry.sav" "$out/legacy-dry.sav"
    sha256sum "$out/legacy-dry.sav"
fi

for frame in 352 645; do
    grep -Fx "TEST deep-water-route $frame DeepWaterDryLandProbe exists=1 src=0 dst=0 goal=2255.0,1005.0" "$out/$phase.log"
done
grep -Fx 'TEST deep-water-route 655 DeepWaterDryLandProbe exists=0 src=0 dst=0 goal=2255.0,1005.0' "$out/$phase.log"
grep -E '^TEST deep-water-(route|sample) (352|645|655) DeepWaterDryLandProbe ' "$out/$phase.log" > "$out/$phase.route"

if [ "$phase" = resume ]; then
    diff -u "$out/base.route" "$out/resume.route"
    diff -u "$out/base.crc" "$out/resume.crc"
    printf 'Compared 304 legacy dry frames. Differences 0\n'
fi
