#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d "$build/pkg46-defense-policies.XXXXXX")
printf 'Defense traces %s\n' "$out"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_RETAIL_SKIRMISH_AI=brutal GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
cd /game-install/rotwk

probe() (
    size=$1
    probability=$2
    case "$size" in
        1) field=Small ;;
        2) field=Med ;;
        3) field=Large ;;
        *) exit 2 ;;
    esac
    state="$out/$size-$probability"
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
    cat "$fixtures/high-pass.ini" >> "$mapdir/map.ini"
    cat >> "$mapdir/map.ini" <<INI

ArmyDefinition MordorArmy
    TacticalAITargets = DEFENSIVE
    MaxTeamsPerTarget = 1
    ChanceToUseAllUnitsForDefenseTarget_$field = $probability%
End
INI
    cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
44 power-reveal - - 0
95 create Player_2 Pkg46DefenseAnchor 0 1450,2760,0
95 create Player_2 Pkg46DefenseFootSoldier 0 1850,2450,0
95 create Player_2 Pkg46DefenseFootSoldier 1 1990,2570,0
95 create Player_2 Pkg46DefenseFootSoldier 2 1980,2720,0
95 create Player_2 Pkg46DefenseFootSoldier 3 1970,2900,0
95 create Player_2 Pkg46DefenseFootSoldier 4 1900,3110,0
95 create Player_2 Pkg46DefenseFootSoldier 5 1970,3260,0
100 create Player_1 Pkg46DefenseAttacker 0 1600,2800,180
101 attack Player_1 Pkg46DefenseAttacker 0 Player_2:Pkg46DefenseAnchor:0
310 quit - - 0
COMMANDS
    if [ "$size" -ge 2 ]; then
        printf '%s\n' '95 create Player_2 Pkg46DefenseAnchor 1 1450,2940,0' >> "$GENERALSX_TEST_COMMANDS"
    fi
    if [ "$size" -ge 3 ]; then
        printf '%s\n' '95 create Player_2 Pkg46DefenseAnchor 2 1280,2760,0' >> "$GENERALSX_TEST_COMMANDS"
    fi
    sort -sn "$GENERALSX_TEST_COMMANDS" -o "$GENERALSX_TEST_COMMANDS"
    timeout 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1
    awk -v size="$size" -v probability="$probability" '
        /^TEST frame 95: create Pkg46DefenseAnchor/ { split($8, id, "="); anchor[id[2]] = 1 }
        /^TEST frame 100: create Pkg46DefenseAttacker/ { split($8, id, "="); attacker = id[2] }
        $1 == "AI_DEFENSE" && $3 == 4 && $4 in anchor && $5 == attacker && !frame {
            frame = $2
            buildings = $6
            all = $7
            recruited = $8
        }
        END { print size, probability, frame + 0, buildings + 0, all + 0, recruited + 0 }
    ' "$state/run.log"
)

printf 'size probability frame buildings all-mode recruited\n'
for size in 1 2 3; do
    probe "$size" 0
    probe "$size" 100
done | tee "$out/decisions.txt"

awk '
    $2 == 0 { frame[$1] = $3; size[$1] = $4; recruited[$1] = $6; bad += ($3 == 0 || $5 != 0) }
    $2 == 100 {
        seen++
        bad += ($3 != frame[$1] || $4 != $1 || size[$1] != $1 || $5 != 1 || $6 != 4 || $6 == recruited[$1])
    }
    END { exit bad || seen != 3 }
' "$out/decisions.txt"
