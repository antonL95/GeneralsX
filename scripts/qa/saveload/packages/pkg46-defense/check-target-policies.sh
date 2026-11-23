#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
out=$(mktemp -d "$build/pkg46-target-policies.XXXXXX")
printf 'Policy traces %s\n' "$out"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_RETAIL_SKIRMISH_AI=brutal GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
cd /game-install/rotwk

probe() (
    label=$1
    targets=ENEMY_STRUCTURE
    teams=2
    seconds=4
    retention=0%
    offense=1
    case "$label" in
        control) ;;
        targets) targets=DEFENSIVE ;;
        teams) teams=1 ;;
        lifetime) seconds=0 ;;
        retention) retention=100% ;;
        offense) offense=0 ;;
        *) exit 2 ;;
    esac
    state="$out/$label"
    export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
    export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
    export GENERALSX_TEST_COMMANDS="$state/tests.txt"
    map='map mp high pass'
    installed="/game-install/aotr/maps/$map"
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$map"
    mkdir -p "$mapdir"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    cp "$installed/$map.map" "$installed/map.ini" "$mapdir/"
    cat >> "$mapdir/map.ini" <<INI

ArmyDefinition MordorArmy
    TacticalAITargets = $targets
    MaxTeamsPerTarget = $teams
    SecondsTillTargetsCanExpire = $seconds
    ChanceForTargetToExpire = $retention
End

SkirmishAIData Pkg46TargetPolicies
    DifficultyTuning BrutalTuning
        OffensiveTacticActivationProbability = $offense : 1
    End
End
INI
    cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
44 power-reveal - - 0
45 create Player_1 GondorFarm 0 2180,3050,0
45 create Player_2 MordorFighterHorde 0 1940,3100,0
45 create Player_2 MordorFighterHorde 1 1950,3150,0
45 create Player_2 MordorFighterHorde 2 1950,3200,0
96 create Player_2 MordorFighterHorde 3 1650,2700,0
96 create Player_2 MordorFighterHorde 4 1770,2840,0
96 create Player_2 MordorFighterHorde 5 1930,2790,0
186 quit - - 0
COMMANDS
    timeout 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1
    awk -v label="$label" '
        /^TEST frame 45: create GondorFarm for Player_1/ { split($8, id, "="); target = id[2] }
        $1 == "AI_WAVE" && $3 == 4 && $4 == "structure" && $6 == target { wave[$2]++ }
        $1 == "AI_TARGET_EXPIRE" && $3 == 4 && $5 == target && !expiry { expiry = $2 }
        END {
            if (!target) exit 1
            print label, target, wave[64] + 0, wave[124] + 0, expiry + 0
        }' "$state/run.log"
)

printf 'policy target frame64-wave frame124-wave first-expiry\n'
for label in control targets teams lifetime retention offense; do
    probe "$label"
done | tee "$out/decisions.txt"

awk '
    $1 == "control"   { seen++; bad += ($3 != 1 || $4 != 1 || $5 != 184) }
    $1 == "targets"   { seen++; bad += ($3 != 0 || $4 != 0 || $5 != 0) }
    $1 == "teams"     { seen++; bad += ($3 != 1 || $4 != 0 || $5 != 184) }
    $1 == "lifetime"  { seen++; bad += ($3 != 1 || $4 != 0 || $5 != 124) }
    $1 == "retention" { seen++; bad += ($3 != 1 || $4 != 1 || $5 != 0) }
    $1 == "offense"   { seen++; bad += ($3 != 0 || $4 != 0 || $5 != 0) }
    END { exit bad || seen != 6 }
' "$out/decisions.txt"
