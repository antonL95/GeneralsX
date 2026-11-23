#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d "$build/skirmish-tactical-policies.XXXXXX")
printf 'Policy traces %s\n' "$work"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_RETAIL_SKIRMISH_AI=brutal GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
cd /game-install/rotwk

probe() (
    label=$1
    state="$work/$label"
    export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
    export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
    export GENERALSX_TEST_COMMANDS="$state/tests.txt"
    map='map mp high pass'
    installed="/game-install/aotr/maps/$map"
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$map"
    mkdir -p "$mapdir"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    cp "$installed/$map.map" "$installed/map.ini" "$mapdir/"
    printf '\n' >> "$mapdir/map.ini"

    case "$label" in
        forward|no-targetless|unavailable-building|ordinary-building)
            defense="$fixtures/../pkg46-defense"
            cat "$defense/high-pass.ini" "$defense/high-pass-alone.ini" >> "$mapdir/map.ini"
            cat "$defense/high-pass.txt" > "$GENERALSX_TEST_COMMANDS"
            case "$label" in
                no-targetless)
                    printf '\nArmyDefinition MordorArmy\n    TacticalAITargets = DEFENSIVE ENEMY_STRUCTURE\nEnd\n' >> "$mapdir/map.ini"
                    ;;
                unavailable-building)
                    printf '\nArmyDefinition MordorArmy\n    OffensiveBuildings = MenWallHubSmall\nEnd\n' >> "$mapdir/map.ini"
                    ;;
                ordinary-building)
                    cat >> "$mapdir/map.ini" <<'INI'

ArmyDefinition MordorArmy
    TacticalAITargets = DEFENSIVE ENEMY_STRUCTURE
    OffensiveBuildings = MordorBattleTower
    ScavangedResourceBuildings = MordorBattleTower
End
INI
                    ;;
            esac
            printf '320 quit - - 0\n' >> "$GENERALSX_TEST_COMMANDS"
            ;;
        opportunity|no-offense)
            cat "$fixtures/high-pass.ini" >> "$mapdir/map.ini"
            awk '$1 < 424' "$fixtures/high-pass.txt" > "$GENERALSX_TEST_COMMANDS"
            if [ "$label" = no-offense ]; then
                cat >> "$mapdir/map.ini" <<'INI'

SkirmishAIData SkirmishOpportunityControl
    DifficultyTuning BrutalTuning
        OffensiveTacticActivationProbability = 0 : 1
    End
End
INI
            fi
            printf '425 quit - - 0\n' >> "$GENERALSX_TEST_COMMANDS"
            ;;
        priority|no-growth)
            cat "$fixtures/high-pass.ini" >> "$mapdir/map.ini"
            magnitude=450
            if [ "$label" = no-growth ]; then
                magnitude=0
            fi
            cat >> "$mapdir/map.ini" <<INI

ArmyDefinition MordorArmy
    EconomyBuilderMinFarmsOwned = 0
    ScavangedResourceBuildings =
    PhaseDuration_Rush = 30
    MustUseCommandPointPercentage_Phase1 = 100%
    LowUnitPriorityModifier_Rush = $magnitude
    FortressRebuildPriority = 1450
End
INI
            cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
45 pkg23-set-cash Player_2 - 0 0
45 create Player_2 SkirmishPriorityProducer 0 2940,3520,0
165 destroy Player_2 MordorFortressCitadel 0
166 create Player_2 MordorPorter 0 2800,3240,0
180 pkg23-set-cash Player_2 - 0 10000
186 quit - - 0
COMMANDS
            ;;
    esac

    timeout 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1
    awk -v label="$label" '
        $1 == "AI_FORWARD_BUILD" && $3 == 4 && !built { built = $2; building = $4; x = $13; y = $14 }
        $1 == "AI_BUILD" && $3 == 4 && $5 == "MordorBattleTower" && !ordinary { ordinary = $2 }
        /^TEST frame 342: create SkirmishOpportunityBuilder for Player_1 .*2740.0, 3070.0/ {
            split($8, id, "=")
            target = id[2]
        }
        $1 == "AI_WAVE" && $3 == 4 && $4 == "opportunity" && !wave { wave = $2; picked = $6 }
        $1 == "AI_UNIT_REQUEST" && $2 == 184 && $3 == 4 && $4 == "SkirmishPendingHorde" { previous = $6; priority = $7 }
        $2 == 184 && $3 == 4 && ($1 == "AI_BUILD" || $1 == "AI_TRAIN") && !action { action = $1; payload = $5 }
        END {
            printf "%s %d %s %g %g %d %d %d %g %g %s %s %d\n", label, built, building ? building : "-", x, y,
                wave, picked, target, previous, priority, action ? action : "-", payload ? payload : "-", ordinary
        }
    ' "$state/run.log"
)

printf 'policy build-frame building x y wave-frame selected-builder expected-builder previous-priority priority first-order-at-184 template ordinary-tower-frame\n'
for label in forward no-targetless unavailable-building ordinary-building opportunity no-offense priority no-growth; do
    probe "$label"
done | tee "$work/decisions.txt"

awk '
    $1 == "forward" { seen++; bad += ($2 != 244 || $3 != "MordorBattleTower") }
    $1 == "no-targetless" || $1 == "unavailable-building" { seen++; bad += ($2 != 0) }
    $1 == "ordinary-building" { seen++; bad += ($2 != 0 || !$13) }
    $1 == "opportunity" { seen++; bad += ($6 != 364 || !$8 || $7 != $8) }
    $1 == "no-offense" { seen++; bad += ($6 != 0 || !$8) }
    $1 == "priority" { seen++; bad += ($9 <= 500 || $10 <= 1450 || $11 != "AI_TRAIN" || $12 != "SkirmishPendingHorde") }
    $1 == "no-growth" { seen++; bad += ($9 != 500 || $10 != 500 || $11 != "AI_BUILD" || $12 != "MordorFortress") }
    END { exit bad || seen != 8 }
' "$work/decisions.txt"
