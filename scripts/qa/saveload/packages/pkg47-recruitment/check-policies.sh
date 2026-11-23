#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
out=$(mktemp -d "$build/pkg47-policies.XXXXXX")
printf 'Policy traces %s\n' "$out"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_RETAIL_SKIRMISH_AI=brutal GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
cd /game-install/rotwk

probe() (
    phase=$1
    label=$2
    field=${3:-}
    value=${4:-}
    state="$out/$phase-$label"
    export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
    export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
    export GENERALSX_TEST_COMMANDS="$state/tests.txt"
    map='map mp high pass'
    installed="/game-install/aotr/maps/$map"
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$map"
    mkdir -p "$mapdir"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    cp "$installed/$map.map" "$installed/map.ini" "$mapdir/"
    case "$phase" in
        rush) rush=30; midgame=30 ;;
        mid) rush=1; midgame=30 ;;
        end) rush=1; midgame=1 ;;
    esac
    cat >> "$mapdir/map.ini" <<INI

ArmyDefinition MordorArmy
    EconomyBuilderMinFarmsOwned = 0
    ScavangedResourceBuildings =
    PhaseDuration_Rush = $rush
    PhaseDuration_MidGame = $midgame
    DefaultUnitPriority = 500
    MustUseCommandPointPercentage_Phase1 = 30%
    MustUseCommandPointPercentage_Phase2 = 60%
    MustUseCommandPointPercentage_Phase3 = 90%
    LowUnitPriorityModifier_Rush = 100
    LowUnitPriorityModifier_MidGame = 200
    LowUnitPriorityModifier_EndGame = 300
INI
    if [ -n "$field" ]; then
        printf '    %s = %s\n' "$field" "$value" >> "$mapdir/map.ini"
    fi
    printf 'End\n' >> "$mapdir/map.ini"
    cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
45 create Player_2 MordorOrcPit 0 2770,3400,0
45 cash Player_2 - 0 10000
66 quit - - 0
COMMANDS
    timeout 200 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1
    awk -F '\t' -v label="$phase-$label" '
        $2 == 64 && $3 == 4 && $1 == "AI_UNIT_PRIORITY" { priority = $9 }
        $2 == 64 && $3 == 4 && ($1 == "AI_BUILD" || $1 == "AI_TRAIN") && !action { action = $1; template = $5 }
        END {
            if (!priority || !action) exit 1
            print label, priority, action, template
        }' "$state/run.log"
)

check_phase() {
    phase=$1
    threshold=$2
    modifier=$3
    probe "$phase" control > "$out/$phase-control.txt"
    probe "$phase" threshold "$threshold" 0% > "$out/$phase-threshold.txt"
    probe "$phase" modifier "$modifier" 0 > "$out/$phase-modifier.txt"
    for label in control threshold modifier; do
        cat "$out/$phase-$label.txt"
    done
    awk '
        $1 ~ /-control$/ { found++; if ($3 != "AI_TRAIN") bad = 1 }
        $1 !~ /-control$/ { found++; if ($3 != "AI_BUILD") bad = 1 }
        END { exit bad || found != 3 }
    ' "$out/$phase-control.txt" "$out/$phase-threshold.txt" "$out/$phase-modifier.txt"
}

check_phase rush MustUseCommandPointPercentage_Phase1 LowUnitPriorityModifier_Rush
probe rush default DefaultUnitPriority 600 > "$out/rush-default.txt"
cat "$out/rush-default.txt"
awk '$3 == "AI_BUILD" { found = 1 } END { exit !found }' "$out/rush-default.txt"
check_phase mid MustUseCommandPointPercentage_Phase2 LowUnitPriorityModifier_MidGame
check_phase end MustUseCommandPointPercentage_Phase3 LowUnitPriorityModifier_EndGame
