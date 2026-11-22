#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
    printf '%s\n' "Usage: $0 BUILD_DIR {training|high-pass|zero-hour} SAVE_FRAME FRAME_COUNT" >&2
    exit 2
fi

build=$(cd "$1" && pwd)
mode=$2
save_frame=$3
count=$4
for number in "$save_frame" "$count"; do
    case "$number" in ''|0*|*[!0-9]*) exit 2 ;; esac
done
end=$((save_frame + count))
fixtures=$(cd "$(dirname "$0")/saveload" && pwd)
out=$(mktemp -d "$build/pkgs1-$mode.XXXXXX")
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
work=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
engine="$build/GeneralsMD/GeneralsXZH"
case "$mode" in
    training)
        scenario="$fixtures/training.txt"
        package="$aotr/UserData/Maps/AotR Training Grounds"
        mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/aotr training grounds"
        mkdir -p "$mapdir"
        map="$mapdir/aotr training grounds.map"
        cp "$package/AotR Training Grounds.map" "$map"
        cp "$package/map.ini" "$package/map.str" "$mapdir/"
        printf '\n' >> "$mapdir/map.ini"
        cat "$fixtures/training.ini" >> "$mapdir/map.ini"
        cat "$fixtures/training.gdb" > "$out/start.gdb"
        sed '/^break ScriptEngine::startEndGameTimer/,$!d' "$fixtures/training.gdb" > "$out/resume.gdb"
        ;;
    high-pass)
        scenario="$fixtures/high-pass.txt"
        map='maps/map mp high pass/map mp high pass.map'
        touch "$out/start.gdb" "$out/resume.gdb"
        ;;
    zero-hour)
        work=${PKGS1_ZERO_HOUR_DATA:?Zero Hour game directory required}
        map=${PKGS1_ZERO_HOUR_MAP:?Zero Hour map required}
        scenario=/dev/null
        touch "$out/start.gdb" "$out/resume.gdb"
        ;;
    *) exit 2 ;;
esac

diag=${PKGS1_DIAGNOSTICS:-"$save_frame,$((save_frame + 1))"}
{
    awk -v end="$end" '$1 <= end && $2 != "quit" && $2 != "xpcrc"' "$scenario"
    printf '%s\n' "$diag" | tr ',' '\n' | awk -v end="$end" '/^[0-9]+$/ && $1 <= end {print $1, "snapshot-crc - - 0"; print $1, "logall - - 0"}'
    seq "$save_frame" "$end" | awk '{ print $1, "xpcrc - - 0" }'
    printf '%s quit - - 0\n' "$end"
} | sort -sn > "$out/continue.txt"

awk -v frame="$save_frame" '$1 >= frame' "$out/continue.txt" > "$out/resume.txt"
{
    awk -v frame="$save_frame" '$1 < frame && $2 != "xpcrc" && $2 != "snapshot-crc" && $2 != "logall" && $2 != "quit"' "$out/continue.txt"
    printf '%s snapshot-save - - 0 parity.sav\n' "$save_frame"
    printf '%s quit - - 0\n' "$((save_frame + 1))"
} > "$out/save.txt"

cd "$work"
run() {
    phase=$1
    gdb_file=$2
    shift 2
    export GENERALSX_TEST_COMMANDS="$out/$phase.txt"
    if [ "$mode" != zero-hour ]; then
        set -- -mod "$aotr" -bfme2 "$bfme2" "$@"
    fi
    timeout --signal=INT --kill-after=10 360 env -u HOME gdb -q -nx -batch -return-child-result \
        -x "$out/$gdb_file.gdb" -ex run --args "$engine" -headless -noFPSLimit \
        -noaudio -nomusic -novideo -noshellmap "$@" > "$out/$phase.log" 2>&1 || {
        tail -40 "$out/$phase.log" >&2
        return 1
    }
}
expect() {
    grep -Fx "$2" "$out/$1.log" || {
        tail -40 "$out/$1.log" >&2
        return 1
    }
}
run save start -file "$map"
expect save "TEST snapshot-save frame=$save_frame code=0"
run continue start -file "$map"
expect continue "TEST frame $end: quit"
run resume resume -loadsave parity.sav
expect resume "TEST frame $end: quit"
for phase in continue resume; do
    awk -v first="$save_frame" -v last="$end" '
        /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
            split($3, f, "=")
            if (f[2] != first + count++ || length($4) != 12) exit 1
            print f[2], $4
        }
        END {if (count != last - first + 1) exit 1}
    ' "$out/$phase.log" > "$out/$phase.crc"
done
paste "$out/continue.crc" "$out/resume.crc" | awk '
    $1 != $3 || $2 != $4 { if (!different++) print "First mismatch", $0 }
    END { print "Compared " NR " frames. Differences " different + 0; exit (different > 0) }
'
