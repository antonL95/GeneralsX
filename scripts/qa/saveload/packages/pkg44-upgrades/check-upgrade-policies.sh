#!/bin/sh
set -eu

fixture=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
build=$(cd "${1:?build directory required}" && pwd)
out=$(mktemp -d "$build/pkg44-policies.XXXXXX")
printf 'Policy traces %s\n' "$out"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_SKIRMISH_AI_TRACE=1 SDL_AUDIO_DRIVER=dummy
export GENERALSX_SKIRMISH_PROBE_A=FactionMen GENERALSX_SKIRMISH_PROBE_B=FactionWild
export GENERALSX_SKIRMISH_PROBE_FRAMES=1280
cd /game-install/rotwk

probe() (
    difficulty=$1
    variant=$2
    army_policy=
    power_policy=
    normal_race=
    case "$variant" in
        control|repeat) ;;
        unit-chance) army_policy='ChanceForUnitsToUpgrade = 0%' ;;
        unit-low) army_policy='UnitUpgradePriorityLow = 600' ;;
        unit-high) army_policy='UnitUpgradePriorityHigh = 400' ;;
        important-low) army_policy='UpgradeSciencePriorityImportantLow = 750' ;;
        important-high) army_policy='UpgradeSciencePriorityImportantHigh = 500' ;;
        normal-low) army_policy='UpgradeSciencePriorityNormalLow = 250' ;;
        normal-high) army_policy='UpgradeSciencePriorityNormalHigh = 100' ;;
        normal-race-control|normal-race-low|normal-race-high)
            normal_race='UnitUpgradePriorityLow = 175
    UnitUpgradePriorityHigh = 175'
            case "$variant" in
                normal-race-low) army_policy='UpgradeSciencePriorityNormalLow = 250' ;;
                normal-race-high) army_policy='UpgradeSciencePriorityNormalHigh = 100' ;;
            esac
            ;;
        power-chance) power_policy='SpecialPowerActivationProbability = 0 : 1' ;;
        *) exit 2 ;;
    esac
    state="$out/$difficulty-$variant"
    export XDG_DATA_HOME="$state/data" XDG_CONFIG_HOME="$state/config"
    export XDG_CACHE_HOME="$state/cache" XDG_RUNTIME_DIR="$state/runtime"
    export GENERALSX_TEST_COMMANDS="$state/tests.txt"
    export GENERALSX_SKIRMISH_PROBE_DIFFICULTY="$difficulty"
    map='map mp high pass'
    installed="/game-install/aotr/maps/$map"
    mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$map"
    mkdir -p "$mapdir"
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    cp "$installed/$map.map" "$installed/map.ini" "$mapdir/"
    printf '\n' >> "$mapdir/map.ini"
    cat "$fixture/high-pass.ini" >> "$mapdir/map.ini"
    cat >> "$mapdir/map.ini" <<INI

ArmyDefinition WildArmy
    PhaseDuration_Rush = 5
    PhaseDuration_MidGame = 5
    $normal_race
    $army_policy
End
INI
    if [ -n "$power_policy" ]; then
        cat >> "$mapdir/map.ini" <<INI

SkirmishAIData Pkg44PowerPolicy
    DifficultyTuning BrutalTuning
        $power_policy
    End
End
INI
    fi
    cp "$fixture/high-pass.txt" "$GENERALSX_TEST_COMMANDS"
    printf '%s\n' '75 pkg44-points Player_2 - 0 50' >> "$GENERALSX_TEST_COMMANDS"
    if [ -n "$normal_race" ]; then
        printf '%s\n' '241 create Player_2 Pkg44ArmoredUnit 1 2020,3410,0' >> "$GENERALSX_TEST_COMMANDS"
    fi
    timeout 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$mapdir/$map.map" > "$state/run.log" 2>&1

    awk -v difficulty="$difficulty" -v variant="$variant" '
        $1 == "SKIRMISH_PLAYER" && $4 == "FactionWild" { player = $3 }
        $1 == "AI_UPGRADE" && $6 == "Pkg44UnitArmor" && !unit { unit = $2; unitScore = $7; if (!first) first = $4 }
        $1 == "AI_UPGRADE" && $6 == "Pkg44ImportantResearch" && !important { important = $2; importantScore = $7; if (!first) first = $4 }
        $1 == "AI_UPGRADE" && $6 == "Pkg44NormalResearch" && !normal { normal = $2; normalScore = $7 }
        $1 == "AI_UPGRADE" && $2 == 244 && ($6 == "Pkg44UnitArmor" || $6 == "Pkg44NormalResearch") && !race { race = $4 }
        $1 == "SKIRMISH_PURCHASE" && sciences[$3] < 2 { science[$3, ++sciences[$3]] = $4 "@" $2 }
        $1 == "SKIRMISH_CAST" && !firstCast[$3] { firstCast[$3] = $4 "@" $2 }
        $1 == "SKIRMISH_CAST" && $4 == "Pkg44CapturePower" && !capture[$3] { capture[$3] = $2 }
        $1 == "SKIRMISH_END" { ended = 1 }
        END {
            if (!ended || !player) exit 1
            if (variant == "unit-chance" ? unit : !unit) exit 1
            if (!important || !normal || (variant == "power-chance" ? firstCast[player] : !capture[player])) exit 1
            printf "%s\t%s\t%d\t%g\t%d\t%g\t%d\t%g\t%s\t%s\t%s\t%d\t%s\t%s\n", difficulty, variant,
                unit, unitScore, important, importantScore, normal, normalScore, science[player, 1] ? science[player, 1] : "-",
                science[player, 2] ? science[player, 2] : "-", firstCast[player] ? firstCast[player] : "-", capture[player], first, race ? race : "-"
        }' "$state/run.log"
)

printf 'difficulty\tvariant\tunit frame\tunit priority\timportant frame\timportant priority\tnormal frame\tnormal priority\tscience 1\tscience 2\tfirst cast\tcapture frame\tfirst order\tframe 244 order\n'
for difficulty in easy medium hard brutal; do
    probe "$difficulty" control
done > "$out/decisions.tsv"
for variant in repeat unit-chance unit-low unit-high important-low important-high normal-low normal-high power-chance; do
    probe brutal "$variant"
done >> "$out/decisions.tsv"
for variant in unit-high important-low normal-race-control normal-race-low normal-race-high; do
    probe medium "$variant"
done >> "$out/decisions.tsv"
cat "$out/decisions.tsv"

awk '
    $1 == "brutal" && $2 == "control"   { unit = $3; important = $5; normal = $7; capture = $12 }
    $1 == "medium" && $2 == "control"   { mediumUnit = $3; mediumResearch = $5; mediumFirst = $13 }
    $1 == "medium" && ($2 == "unit-high" || $2 == "important-low") {
        contested++; bad += ($3 != mediumUnit || $5 != mediumResearch || mediumFirst != "unit" || $13 != "research")
    }
    $2 == "normal-race-control" { race = $14 }
    $2 == "normal-race-low"     { raceLow = $14 }
    $2 == "normal-race-high"    { raceHigh = $14 }
    $1 != "brutal" { next }
    $2 == "unit-chance"    { seen++; bad += ($3 != 0 || $5 != important || $7 != normal) }
    $2 == "unit-low"       { seen++; bad += ($3 != unit || $4 != 600) }
    $2 == "unit-high"      { seen++; bad += ($3 != unit || $4 != 400) }
    $2 == "important-low"  { seen++; bad += ($5 != important || $6 != 750) }
    $2 == "important-high" { seen++; bad += ($5 != important || $6 != 500) }
    $2 == "normal-low"     { seen++; bad += ($7 != normal || $8 != 250) }
    $2 == "normal-high"    { seen++; bad += ($7 != normal || $8 != 100) }
    $2 == "power-chance"   { seen++; bad += (!capture || $11 != "-" || $12 != 0) }
    END {
        exit bad || seen != 8 || contested != 2 || !race || race == "-" || raceLow != "research" || raceHigh != "unit"
    }
' "$out/decisions.tsv"
for variant in control repeat; do
    awk '$1 == "SKIRMISH_CRC" || $1 == "SKIRMISH_CAST" || $1 == "SKIRMISH_PURCHASE" || $1 == "AI_UPGRADE"' \
        "$out/brutal-$variant/run.log" > "$out/brutal-$variant/trace.tsv"
done
diff -u "$out/brutal-control/trace.tsv" "$out/brutal-repeat/trace.tsv"
