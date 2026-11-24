#!/bin/sh
set -eu
export LC_ALL=C

build=$1
out=$2
phase=$3
case "$phase" in base|resume) ;; *) exit 2 ;; esac

fixtures=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
export XDG_DATA_HOME="$out/$phase/data" XDG_CONFIG_HOME="$out/$phase/config"
export XDG_CACHE_HOME="$out/$phase/cache" XDG_RUNTIME_DIR="$out/$phase/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
gameData="$XDG_DATA_HOME/GeneralsX/GeneralsZH"
mapName='map mp river isen'
map="$gameData/Maps/$mapName/$mapName.map"
aotr=/game-install/aotr
mkdir -p "$gameData/Save" "$(dirname "$map")"
cp "$aotr/maps/$mapName/$mapName.map" "$aotr/maps/$mapName/map.ini" "$(dirname "$map")/"

if [ "$phase" = base ]; then
    cp "$fixtures/training.gdb" "$out/$phase.gdb"
    printf '285 snapshot-save - - 0 legacy-isen.sav\n' > "$out/$phase.txt"
    set -- -file "$map"
else
    cp "$out/legacy-isen.sav" "$gameData/Save/"
    sed '/^break ScriptEngine::startEndGameTimer/,$!d' "$fixtures/training.gdb" > "$out/$phase.gdb"
    : > "$out/$phase.txt"
    set -- -loadsave legacy-isen.sav
fi

{
    seq 285 325 | awk '{ print $1, "xpcrc - - 0" }'
    printf '325 quit - - 0\n'
} >> "$out/$phase.txt"

export GENERALSX_TEST_COMMANDS="$out/$phase.txt"
cd /game-install/rotwk
timeout --signal=INT --kill-after=10 360 env -u HOME gdb -q -nx -batch -return-child-result \
    -x "$out/$phase.gdb" -ex run --args "$build/GeneralsMD/GeneralsXZH" \
    -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap -mod "$aotr" \
    -bfme2 /game-install/bfme2 "$@" > "$out/$phase.log" 2>&1 || {
    tail -40 "$out/$phase.log" >&2
    exit 1
}
grep -Fx 'TEST frame 325: quit' "$out/$phase.log"
awk '
    /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
        split($3, f, "=")
        if (f[2] != 285 + count++ || length($4) != 12) exit 1
        print f[2], $4
    }
    END { if (count != 41) exit 1 }
' "$out/$phase.log" > "$out/$phase.crc"

if [ "$phase" = base ]; then
    grep -Fx 'TEST snapshot-save frame=285 code=0' "$out/$phase.log"
    cp "$gameData/Save/legacy-isen.sav" "$out/legacy-isen.sav"
else
    paste "$out/base.crc" "$out/resume.crc" | awk '
        $1 != $3 || $2 != $4 { if (!different++) print "First mismatch", $0 }
        END { print "Compared " NR " legacy frames. Differences " different + 0; exit (NR != 41 || different > 0) }
    '
fi
