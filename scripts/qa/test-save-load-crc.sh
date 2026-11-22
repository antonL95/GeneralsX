#!/bin/sh
set -eu
export LC_ALL=C

usage() {
    printf '%s\n' "Usage: $0 [--only PACKAGE] BUILD_DIR {training|high-pass|spellbook|marsh-training|marsh-stock|zero-hour} SAVE_FRAME FRAME_COUNT" >&2
    exit 2
}

only=
if [ "${1:-}" = --only ]; then
    [ "$#" -eq 6 ] || usage
    only=$2
    [ -n "$only" ] || usage
    shift 2
elif [ "$#" -eq 6 ] && [ "$5" = --only ]; then
    only=$6
    [ -n "$only" ] || usage
    set -- "$1" "$2" "$3" "$4"
fi
[ "$#" -eq 4 ] || usage
case "$only" in *[!a-z0-9-]*) usage ;; esac

build=$(cd "$1" && pwd)
mode=$2
save_frame=$3
count=$4
for number in "$save_frame" "$count"; do
    case "$number" in ''|0*|*[!0-9]*) usage ;; esac
done
end=$((save_frame + count))
runner=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
fixtures=$(cd "$(dirname "$0")/saveload" && pwd)
registry=$fixtures
if [ ! -d "$registry/packages" ] && [ -f "$build/CMakeCache.txt" ]; then
    source_dir=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$build/CMakeCache.txt")
    registry=$source_dir/scripts/qa/saveload
fi

case "$mode" in
    training|high-pass|spellbook|zero-hour) fixture_mode=$mode ;;
    marsh-training|marsh-stock) fixture_mode=marsh ;;
    *) usage ;;
esac

packages=
set --
for directory in "$registry"/packages/*; do
    [ -d "$directory" ] || continue
    name=${directory##*/}
    if [ -n "$only" ] && [ "$only" != "$name" ]; then
        continue
    fi
    if [ "$mode" = spellbook ] && [ -z "$only" ]; then
        for file in "$directory"/training.ini "$directory"/training.txt "$directory"/training.cash; do
            [ ! -f "$file" ] || set -- "$@" "$file"
        done
    fi
    found=
    for extension in txt ini cash sh gdb; do
        file=$directory/$fixture_mode.$extension
        if [ -f "$file" ]; then
            found=1
            case "$extension" in txt|ini|cash) set -- "$@" "$file" ;; esac
        fi
    done
    if [ -n "$found" ]; then
        case "$name" in
            *[!a-z0-9-]*) printf 'Invalid fixture package directory: %s\n' "$directory" >&2; exit 2 ;;
        esac
        packages="${packages:+$packages }$name"
        if [ -n "$only" ]; then
            for file in "$directory"/"$fixture_mode"-alone.txt "$directory"/"$fixture_mode"-alone.ini; do
                [ ! -f "$file" ] || set -- "$@" "$file"
            done
        fi
    fi
done
if [ -z "$packages" ] && { [ "$mode" != zero-hour ] || [ -n "$only" ]; }; then
    printf 'No fixture package %s for %s\n' "${only:-registered}" "$mode" >&2
    exit 2
fi
if [ "$#" -gt 0 ]; then
    awk -f "$registry/validate-packages.awk" "$@"
fi

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

copy_map() {
    map_name=$1
    package=$2
    folder=$(printf '%s' "$map_name" | tr '[:upper:]' '[:lower:]')
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$folder"
    mkdir -p "$mapdir"
    map="$mapdir/$folder.map"
    cp "$package/$map_name.map" "$map"
    cp "$package/map.ini" "$mapdir/"
    if [ -f "$package/map.str" ]; then
        cp "$package/map.str" "$mapdir/"
    fi
}

mapdir=
: > "$out/start.gdb"
: > "$out/resume.gdb"
case "$mode" in
    training|spellbook|marsh-training)
        copy_map 'AotR Training Grounds' "$aotr/UserData/Maps/AotR Training Grounds"
        ;;
    high-pass)
        map='maps/map mp high pass/map mp high pass.map'
        for file do
            case "$file" in
                *.ini) copy_map 'map mp high pass' "$aotr/maps/map mp high pass"; break ;;
            esac
        done
        ;;
    marsh-stock)
        copy_map 'map mp druwaith iaur' "${PKGS1_DRUWAITH_DATA:-"$aotr/maps/map mp druwaith iaur"}"
        ;;
    zero-hour)
        work=${PKGS1_ZERO_HOUR_DATA:?Zero Hour game directory required}
        map=${PKGS1_ZERO_HOUR_MAP:?Zero Hour map required}
        ;;
esac

case "$mode" in
    training|spellbook|marsh-*)
        cat "$fixtures/training.gdb" > "$out/start.gdb"
        sed '/^break ScriptEngine::startEndGameTimer/,$!d' "$fixtures/training.gdb" > "$out/resume.gdb"
        ;;
esac

append_ini() {
    if [ -z "$mapdir" ]; then
        printf 'Map INI overlays are unavailable for %s\n' "$mode" >&2
        return 1
    fi
    printf '\n' >> "$mapdir/map.ini"
    cat "$1" >> "$mapdir/map.ini"
}

append_commands() {
    awk -v end="$end" 'NF && $1 <= end && $2 != "quit" && $2 != "xpcrc"' "$1" >> "$out/commands.txt"
}

: > "$out/commands.txt"
for file do
    case "$file" in
        *.ini) append_ini "$file" ;;
        *.txt) append_commands "$file" ;;
    esac
done

if [ "$mode" = training ] && [ -z "$only" ]; then
    [ ! -f "$fixtures/training.ini" ] || append_ini "$fixtures/training.ini"
    [ ! -f "$fixtures/training.txt" ] || append_commands "$fixtures/training.txt"
    [ -z "${PKGS1_TRAINING_EXTRA_INI:-}" ] || append_ini "$PKGS1_TRAINING_EXTRA_INI"
    [ -z "${PKGS1_TRAINING_EXTRA_COMMANDS:-}" ] || append_commands "$PKGS1_TRAINING_EXTRA_COMMANDS"
    if [ -n "${PKGS1_TRAINING_EXTRA_GDB:-}" ]; then
        cat "$PKGS1_TRAINING_EXTRA_GDB" >> "$out/start.gdb"
    fi
fi

package_step() {
    script=$registry/packages/$1/$fixture_mode.sh
    shift
    if [ -f "$script" ]; then
        sh "$registry/package-step.sh" "$script" "$1" "$out" "$save_frame" "$end" "${2:-continue}"
    fi
}

for name in $packages; do
    prefix=$registry/packages/$name/$fixture_mode
    if [ -f "$prefix.gdb" ]; then
        cat "$prefix.gdb" >> "$out/start.gdb"
    fi
    if [ -f "$prefix-resume.gdb" ]; then
        cat "$prefix-resume.gdb" >> "$out/resume.gdb"
    fi
    package_step "$name" prepare >> "$out/commands.txt"
done

diag=${PKGS1_DIAGNOSTICS:-"$save_frame,$((save_frame + 1))"}
{
    printf '%s\n' "$diag" | tr ',' '\n' | awk -v end="$end" '/^[0-9]+$/ && $1 <= end {print $1, "snapshot-crc - - 0"; print $1, "logall - - 0"}'
    seq "${PKGS1_CRC_START_FRAME:-$save_frame}" "$end" | awk '{ print $1, "xpcrc - - 0" }'
    printf '%s quit - - 0\n' "$end"
} >> "$out/commands.txt"
sort -sn "$out/commands.txt" > "$out/continue.txt"

awk -v frame="$save_frame" '$1 >= frame && $2 != "snapshot-path-queue"' "$out/continue.txt" > "$out/resume.txt"
{
    awk -v frame="$save_frame" '
        $1 < frame && $2 != "xpcrc" && $2 != "snapshot-crc" && $2 != "logall" && $2 != "quit"
        $1 == frame && $2 == "snapshot-path-queue"
    ' "$out/continue.txt"
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

check_packages() {
    failed=
    for name in $packages; do
        if ! package_step "$name" check "$1" || ! package_step "$name" check resume || ! package_step "$name" compare; then
            failed="${failed:+$failed }$name"
        fi
    done
    for name in $failed; do
        if [ -z "$only" ]; then
            isolated=$(mktemp -d "$out/only-$name.XXXXXX")
            if PKGS1_STATE_DIR="$isolated" sh "$runner" --only "$name" "$build" "$mode" "$save_frame" "$count" > "$isolated/result.log" 2>&1; then
                printf 'Fixture interference: %s passes alone and fails combined (%s)\n' "$name" "$isolated" >&2
                continue
            fi
            printf 'Fixture failed both alone and combined: %s (%s)\n' "$name" "$isolated" >&2
        else
            printf 'Fixture failed: %s (%s)\n' "$name" "$out" >&2
        fi
    done
    [ -z "$failed" ]
}

if [ "$mode" = spellbook ]; then
    for name in $packages; do
        package_step "$name" after_save
    done
    run resume resume -loadsave parity.sav
    expect resume "TEST frame $end: quit"
    check_packages save
    exit
fi

run continue start -file "$map"
expect continue "TEST frame $end: quit"
run resume resume -loadsave parity.sav
expect resume "TEST frame $end: quit"
check_packages continue

for phase in continue resume; do
    awk -v first="$save_frame" -v last="$end" '
        /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
            split($3, f, "=")
            if (f[2] < first) next
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
