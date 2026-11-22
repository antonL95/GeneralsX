#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
archive=${2:-}
if [ -z "$archive" ]; then
    archive=$(mktemp -d "$build/pkg43-policies.XXXXXX")
    export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
fi
printf '%s\n' "$archive"

probe() (
    label=$1
    state="$archive/$label"
    export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
    export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime" SDL_AUDIO_DRIVER=dummy
    export GENERALSX_RETAIL_SKIRMISH_AI=brutal GENERALSX_SKIRMISH_AI_TRACE=1
    export GENERALSX_TEST_COMMANDS="$state/tests.txt"
    map='map mp high pass'
    installed="/game-install/aotr/maps/$map"
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$map"
    mkdir -p "$mapdir"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    cp "$installed/$map.map" "$installed/map.ini" "$mapdir/"
    case "$label" in
        nofarm|rebuild-control|no-rebuild)
            cat >>"$mapdir/map.ini" <<'INI'

SkirmishAIData TheSkirmishAIData
    DifficultyTuning BrutalTuning
        Difficulty = BRUTAL
        EconomyMaxFarms = 20
        EconomyUpgradeProbability = 0 : 1
    End
End
INI
            ;;
    esac
    case "$label" in
        interval|budget-control|save-rush|save-mid|save-end)
            printf '\nArmyDefinition MordorArmy\n EconomyBuilderMinTimeBetweenFarms_Rush = 0\n'
            if [ "$label" != interval ]; then
                printf ' PhaseDuration_Rush = 28\n PhaseDuration_MidGame = 7\n'
            fi
            case "$label" in
                save-rush) printf ' PercentToSave_Rush = 75%%\n';;
                save-mid) printf ' PercentToSave_MidGame = 75%%\n';;
                save-end) printf ' PercentToSave_EndGame = 75%%\n';;
            esac
            printf 'End\n'
            ;;
        no-lumber)
            printf '\nArmyDefinition MordorArmy\n ScavangedResourceBuildings =\nEnd\n'
            ;;
        no-rebuild)
            printf '\nArmyDefinition MordorArmy\n StructureRebuildPriorityModifier = 0%%\nEnd\n'
            ;;
        rate-control|rate-drop)
            printf '\nArmyDefinition MordorArmy\n EconomyBuilderMinFarmsOwned = 1\n PhaseDuration_Rush = 31\n PhaseDuration_MidGame = 30\n DefaultUnitPriority = 1000\n'
            if [ "$label" = rate-drop ]; then
                printf ' EconomyBuilderPerSecPriorityIncreaseBase = -3000\n'
            fi
            printf 'End\n'
            ;;
    esac >>"$mapdir/map.ini"
    case "$label" in
        no-lumber) printf '600 quit - - 0\n';;
        rebuild-control|no-rebuild) printf '80 kill Player_2 MordorOrcPit 0\n600 quit - - 0\n';;
        rate-control|rate-drop) printf '932 cash Player_2 - 0 6000\n1600 quit - - 0\n';;
        *) printf '820 heroes Player_2 - 0\n1000 heroes Player_2 - 0\n1120 heroes Player_2 - 0\n1180 quit - - 0\n';;
    esac >"$GENERALSX_TEST_COMMANDS"
    cd /game-install/rotwk
    timeout 240 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" >"$state/run.log" 2>&1
)

if [ "$#" -eq 1 ]; then
    for label in default nofarm interval budget-control save-rush save-mid save-end no-lumber rebuild-control no-rebuild rate-control rate-drop; do
        probe "$label"
    done
fi

expect() {
    label=$1
    shift
    if ! grep -Fq "$*" "$archive/$label/run.log"; then
        printf '%s missing %s\n' "$label" "$*" >&2
        return 1
    fi
}

expect_build() {
    label=$1
    frame=$2
    template=$3
    if ! awk -F '\t' -v frame="$frame" -v template="$template" \
        '$1 == "AI_BUILD" && $2 == frame && $3 == 4 && $5 == template { found = 1 } END { exit !found }' "$archive/$label/run.log"; then
        printf '%s did not build %s at frame %s\n' "$label" "$template" "$frame" >&2
        return 1
    fi
}

expect_build default 4 MordorSlaughterHouse_ForAI
expect_build nofarm 4 MordorLumberMill_ForAI
expect_build default 184 MordorLumberMill_ForAI
expect_build interval 184 MordorSlaughterHouse_ForAI
expect_build no-lumber 184 MordorOrcPit
expect_build rebuild-control 64 MordorOrcPit
expect_build no-rebuild 64 MordorOrcPit
expect_build rebuild-control 124 MordorOrcPit
expect_build no-rebuild 124 MordorHaradrimPalace
expect_build rate-control 1204 MordorSlaughterHouse_ForAI
expect_build rate-drop 1204 MordorHaradrimPalace

expect budget-control "$(printf 'AI_PHASE\t844\t4\t1\t318\t13\t3')"
expect save-rush 'TEST heroes 820 Player_2 money=33 '
expect save-mid 'TEST heroes 820 Player_2 money=322 '
expect save-mid "$(printf 'AI_PHASE\t844\t4\t1\t81\t250\t3')"
expect budget-control "$(printf 'AI_PHASE\t1084\t4\t2\t294\t30\t4')"
expect save-end 'TEST heroes 1000 Player_2 money=280 '
expect save-end "$(printf 'AI_PHASE\t1084\t4\t2\t76\t248\t4')"
printf '%s\n' 'Package 43 policy comparisons passed'
