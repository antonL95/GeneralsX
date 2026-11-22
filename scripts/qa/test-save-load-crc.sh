#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
    printf '%s\n' "Usage: $0 BUILD_DIR {training|high-pass|spellbook|zero-hour} SAVE_FRAME FRAME_COUNT" >&2
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
out=${PKGS1_STATE_DIR:-}
if [ -n "$out" ]; then
    mkdir -p "$out"
    out=$(cd "$out" && pwd)
else
    out=$(mktemp -d "$build/pkgs1-$mode.XXXXXX")
fi
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
work=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
engine="$build/GeneralsMD/GeneralsXZH"
path_unit=
wall=
case "$mode" in
    training|spellbook)
        scenario="$fixtures/training.txt"
        package="$aotr/UserData/Maps/AotR Training Grounds"
        mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/aotr training grounds"
        mkdir -p "$mapdir"
        map="$mapdir/aotr training grounds.map"
        cp "$package/AotR Training Grounds.map" "$map"
        cp "$package/map.ini" "$package/map.str" "$mapdir/"
        printf '\n' >> "$mapdir/map.ini"
        cat "$fixtures/training.ini" >> "$mapdir/map.ini"
        if [ "$mode" = training ] && [ -n "${PKGS1_TRAINING_EXTRA_INI:-}" ]; then
            printf '\n' >> "$mapdir/map.ini"
            cat "$PKGS1_TRAINING_EXTRA_INI" >> "$mapdir/map.ini"
        fi
        cat "$fixtures/training.gdb" > "$out/start.gdb"
        sed '/^break ScriptEngine::startEndGameTimer/,$!d' "$fixtures/training.gdb" > "$out/resume.gdb"
        if [ "$mode" = training ] && [ -n "${PKGS1_TRAINING_EXTRA_GDB:-}" ]; then
            cat "$PKGS1_TRAINING_EXTRA_GDB" >> "$out/start.gdb"
        fi
        if [ "$mode" = spellbook ]; then
            cat "$fixtures/drop-book.gdb" >> "$out/start.gdb"
        else
            path_unit='Player_1 IthilienPathfinder 1'
            if [ "$save_frame" -ge 118 ]; then
                wall='Player_1 GondorCastleWallTrebuchet 0'
            fi
        fi
        ;;
    high-pass)
        scenario="$fixtures/high-pass.txt"
        map='maps/map mp high pass/map mp high pass.map'
        path_unit='Player_1 GondorFighter 0'
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
    if [ "$mode" = training ] && [ -n "${PKGS1_TRAINING_EXTRA_COMMANDS:-}" ]; then
        awk -v end="$end" '$1 <= end && $2 != "quit" && $2 != "xpcrc"' "$PKGS1_TRAINING_EXTRA_COMMANDS"
    fi
    if [ -n "$path_unit" ]; then
        printf '%s snapshot-path-queue %s\n' "$save_frame" "$path_unit"
        printf '%s snapshot-path-state %s\n' "$save_frame" "$path_unit"
        printf '%s snapshot-path-state %s\n' "$((save_frame + 1))" "$path_unit"
    fi
    if [ -n "$wall" ]; then
        printf '%s snapshot-wall-state %s\n' "$save_frame" "$wall"
        printf '%s snapshot-wall-state Player_1 GondorCastleWallSegment 0\n' "$save_frame"
        printf '%s snapshot-wall-occupant Player_1 - 0 WallTrebuchetOccupant\n' "$save_frame"
    fi
    printf '%s\n' "$diag" | tr ',' '\n' | awk -v end="$end" '/^[0-9]+$/ && $1 <= end {print $1, "snapshot-crc - - 0"; print $1, "logall - - 0"}'
    seq "$save_frame" "$end" | awk '{ print $1, "xpcrc - - 0" }'
    printf '%s quit - - 0\n' "$end"
} | sort -sn > "$out/continue.txt"

awk -v frame="$save_frame" '$1 >= frame && $2 != "snapshot-path-queue"' "$out/continue.txt" > "$out/resume.txt"
{
    awk -v frame="$save_frame" '$1 < frame && $2 != "xpcrc" && $2 != "snapshot-crc" && $2 != "logall" && $2 != "quit"' "$out/continue.txt"
    if [ -n "$path_unit" ]; then
        printf '%s snapshot-path-queue %s\n' "$save_frame" "$path_unit"
    fi
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
if [ "$mode" = spellbook ]; then
    grep '^TEST save-load removed spellbook id=[0-9][0-9]* next=[0-9][0-9]*$' "$out/save.log"
    {
        printf '%s snapshot-book-repair Player_1 GondorFighter 0\n' "$save_frame"
        printf '%s quit - - 0\n' "$end"
    } > "$out/resume.txt"
    run resume resume -loadsave parity.sav
    expect resume "TEST frame $end: quit"
    grep '^TEST save-load repaired spellbook ' "$out/resume.log"
    awk '
        /^TEST save-load removed spellbook / { split($6, counter, "="); saved = counter[2] }
        /^TEST save-load repaired spellbook / {
            split($5, book, "="); split($6, unit, "=")
            ok = saved && book[2] == saved && unit[2] > book[2] && $7 == "lookup=1"
        }
        END { exit !ok }
    ' "$out/save.log" "$out/resume.log"
    exit
fi
run continue start -file "$map"
expect continue "TEST frame $end: quit"
run resume resume -loadsave parity.sav
expect resume "TEST frame $end: quit"
for phase in continue resume; do
    if [ "$mode" = training ]; then
        first=0
        if [ "$phase" = resume ]; then
            first=$save_frame
        fi
        awk -v first="$first" -v last="$end" -f "$fixtures/economy.awk" "$out/$phase.log"
    fi
    if [ -n "$path_unit" ]; then
        expect "$phase" "TEST save-load path-state frame=$save_frame waiting=1"
        expect "$phase" "TEST save-load path-state frame=$((save_frame + 1)) waiting=0"
    fi
    if [ -n "$wall" ]; then
        expect "$phase" "TEST save-load wall-state frame=$save_frame template=GondorCastleWallTrebuchet walkable=1"
        expect "$phase" "TEST save-load wall-state frame=$save_frame template=GondorCastleWallSegment walkable=1"
        expect "$phase" "TEST save-load wall-occupant frame=$save_frame name=WallTrebuchetOccupant layer=15 walkable=1"
    fi
    if [ "$mode" = training ] && [ "$end" -ge 126 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 124 ]; }; then
        expect "$phase" 'TEST save-load wall-occupant frame=124 name=WallDeleteOccupant layer=15 walkable=1'
        expect "$phase" 'TEST save-load wall-occupant frame=124 name=WallGateOccupant layer=15 walkable=1'
        expect "$phase" 'TEST save-load wall-occupant frame=126 name=WallDeleteOccupant layer=1 walkable=0'
        expect "$phase" 'TEST save-load wall-occupant frame=126 name=WallGateOccupant layer=1 walkable=0'
        grep '^TEST list 126 Player_1 MenWallGate 0 ' "$out/$phase.log"
    fi
    if [ "$mode" = training ] && [ "$end" -ge 244 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 244 ]; }; then
        grep -E '^TEST pkg19 frame=244 template=Pkg19SaveShooter weapon=Pkg19SaveWindup locked=1 .*draw=272 ' "$out/$phase.log"
        grep -E '^TEST save-load partition frame=244 .* count=[1-9][0-9]*$' "$out/$phase.log" > "$out/$phase.cells"
        test "$(wc -l < "$out/$phase.cells")" -eq 6
        grep -E 'template=MenWallGate .* count=12$' "$out/$phase.cells"
        grep -E 'template=PartitionUpgradeBuilding major=100\.00 ' "$out/$phase.cells"
        grep -E 'template=PartitionScaleFighter major=96\.00 ' "$out/$phase.cells"
        grep -E 'template=PartitionScaleHorde major=120\.00 ' "$out/$phase.cells"
    fi
    if [ "$mode" = training ] && [ "$end" -ge 250 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 250 ]; }; then
        grep -E '^TEST damage frame=250 weapon=Pkg19SaveSecondary .*victim=[0-9]+\(Pkg19SaveTarget\) ' "$out/$phase.log"
    fi
    if [ "$mode" = training ] && [ "$end" -ge 285 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 285 ]; }; then
        expect "$phase" 'TEST frame 285: builderscriptmove object 182 to 1933.7 1774.0'
    fi
    if [ "$mode" = training ] && [ "$end" -ge 450 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 450 ]; }; then
        grep -E '^TEST damage frame=450 weapon=MordorCatapultRockWarhead .*victim=[0-9]+\(MenWallGate\) .*dealt=4\.00 ' "$out/$phase.log"
        grep -E '^TEST health 450 Player_1 MenWallGate 0 id=[0-9]+ health=1495\.99 ' "$out/$phase.log"
    fi
    awk -v first="$save_frame" -v last="$end" '
        /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
            split($3, f, "=")
            if (f[2] != first + count++ || length($4) != 12) exit 1
            print f[2], $4
        }
        END {if (count != last - first + 1) exit 1}
    ' "$out/$phase.log" > "$out/$phase.crc"
done
if [ "$mode" = training ] && [ "$save_frame" -le 244 ] && [ "$end" -ge 244 ]; then
    cmp "$out/continue.cells" "$out/resume.cells"
fi
if [ -n "$path_unit" ]; then
    expect save "TEST save-load path-queue frame=$save_frame queued=1"
    expect continue "TEST save-load path-queue frame=$save_frame queued=1"
fi
paste "$out/continue.crc" "$out/resume.crc" | awk '
    $1 != $3 || $2 != $4 { if (!different++) print "First mismatch", $0 }
    END { print "Compared " NR " frames. Differences " different + 0; exit (different > 0) }
'
