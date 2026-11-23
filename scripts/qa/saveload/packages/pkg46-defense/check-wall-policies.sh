#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
out=$(mktemp -d "$build/pkg46-wall-policies.XXXXXX")
printf 'Wall traces %s\n' "$out"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
export GENERALSX_SKIRMISH_PROBE_A=FactionMen GENERALSX_SKIRMISH_PROBE_B=FactionAngmar
export GENERALSX_SKIRMISH_PROBE_DIFFICULTY=brutal GENERALSX_SKIRMISH_PROBE_FRAMES=280
export GENERALSX_SKIRMISH_PROBE_FIXTURE=human-lair
cd /game-install/rotwk

probe() (
    label=$1
    type=$2
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

ChildObject Pkg46LegalWallNode MenWallHubSmall
    BuildTime = 1
    Geometry = CYLINDER
    GeometryMajorRadius = 10
    GeometryMinorRadius = 10
End

ChildObject Pkg46BlockedWallNode MenWallHubSmall
    BuildTime = 1
    Geometry = CYLINDER
    GeometryMajorRadius = 10000
    GeometryMinorRadius = 10000
End

CommandButton Pkg46BuildLegalWallNode
    Command = DOZER_CONSTRUCT
    Object = Pkg46LegalWallNode
End

CommandButton Pkg46BuildBlockedWallNode
    Command = DOZER_CONSTRUCT
    Object = Pkg46BlockedWallNode
End

CommandSet AngmarPorterCommandSet_ForAI
    11 = Pkg46BuildLegalWallNode
    12 = Pkg46BuildBlockedWallNode
End

ArmyDefinition AngmarArmy
    PhaseDuration_Rush = 1
    PhaseDuration_MidGame = 1
    AIWallNodeAssignment Pkg46WallNode
        TemplateName = $type
    End
End

SkirmishAIData Pkg46WallPolicy
    DisableEconomyBuilding = Yes
    DisableBaseBuilding = Yes
    DisableWallBuilding = No
End
INI
    cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
45 cash Player_2 - 0 10000
COMMANDS
    timeout 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1
    awk -v label="$label" '
        $1 == "AI_WALL_NODE" && $3 == 4 && !frame {
            frame = $2
            defended = $4
            node = $5
            status = $6
        }
        END { print label, frame + 0, defended + 0, node + 0, status }
    ' "$state/run.log"
)

printf 'policy frame defended-structure node outcome\n'
probe eligible Pkg46LegalWallNode | tee "$out/eligible.txt"
probe blocked Pkg46BlockedWallNode | tee "$out/blocked.txt"
awk '
    $1 == "eligible" { seen++; frame = $2; anchor = $3; bad += (!$2 || !$3 || !$4 || $5 != "assigned") }
    $1 == "blocked"  { seen++; bad += ($2 != frame || $3 != anchor || $4 || $5 != "unavailable") }
    END { exit bad || seen != 2 }
' "$out/eligible.txt" "$out/blocked.txt"
