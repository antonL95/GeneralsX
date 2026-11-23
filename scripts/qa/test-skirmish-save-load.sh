#!/bin/sh
set -eu
export LC_ALL=C

usage() {
    printf '%s\n' "Usage: $0 BUILD_DIR {high-pass|lithlad} FACTION_A FACTION_B {medium|hard} SAVE_FRAME END_FRAME" >&2
    exit 2
}

[ "$#" -eq 7 ] || usage
build=$(cd "$1" && pwd)
case "$2" in
    high-pass) map='map mp high pass' ;;
    lithlad) map='map mp lithlad' ;;
    *) usage ;;
esac
export GENERALSX_SKIRMISH_PROBE_A="$3" GENERALSX_SKIRMISH_PROBE_B="$4"
case "$5" in medium|hard) ;; *) usage ;; esac
export GENERALSX_SKIRMISH_PROBE_DIFFICULTY="$5"
first=$6
last=$7
for frame in "$first" "$last"; do
    case "$frame" in ''|0*|*[!0-9]*) usage ;; esac
done
[ "$first" -lt "$last" ] && [ "$last" -le 54000 ] || usage
export GENERALSX_SKIRMISH_PROBE_FRAMES="$last" GENERALSX_SKIRMISH_AI_TRACE=1
export GENERALSX_SKIRMISH_PROBE_CHECKPOINT="$first"
unset GENERALSX_SKIRMISH_PROBE_FIXTURE

out=$(mktemp -d "$build/skirmish-save.XXXXXX")
printf 'Skirmish save and resume state %s\n' "$out"
export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config" XDG_CACHE_HOME="$out/cache"
export XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
mkdir -m 700 "$XDG_RUNTIME_DIR"
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
engine="$build/GeneralsMD/GeneralsXZH"
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}

printf '%s snapshot-save - - 0 parity.sav\n%s quit - - 0\n' "$first" "$((first + 1))" > "$out/save.txt"
{
    printf '%s\n' "${SKIRMISH_SAVE_DIAGNOSTICS:-$first,$((first + 1))}" | tr ',' '\n' |
        awk -v first="$first" -v last="$last" '/^[0-9]+$/ && $1 >= first && $1 <= last { print $1, "snapshot-crc - - 0" }'
    seq "$first" "$last" | awk '{ print $1, "xpcrc - - 0" }'
} | sort -sn > "$out/compare.txt"

cd "${PKGS1_ROTWK_DATA:-/game-install/rotwk}"
run() {
    phase=$1
    commands=$2
    shift 2
    printf 'Running %s\n' "$phase"
    export GENERALSX_TEST_COMMANDS="$out/$commands.txt"
    timeout --kill-after=10 360 "$engine" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod "$aotr" -bfme2 "$bfme2" "$@" > "$out/$phase.log" 2>&1 || {
        tail -40 "$out/$phase.log" >&2
        return 1
    }
}

run save save -file "maps/$map/$map.map"
grep -Fx "TEST snapshot-save frame=$first code=0" "$out/save.log"
run continue compare -file "maps/$map/$map.map"
run resume compare -loadsave parity.sav

for phase in save continue resume; do
    awk -F '\t' '$1 == "SKIRMISH_CHECKPOINT"' "$out/$phase.log" > "$out/$phase.checkpoint"
    [ "$phase" != save ] || continue
    awk -F '\t' -v last="$last" '$1 == "SKIRMISH_END" && $2 == last { seen++ } END { exit (seen != 1) }' "$out/$phase.log" ||
        { printf '%s did not reach skirmish frame %s\n' "$phase" "$last" >&2; exit 1; }
    awk -v first="$first" -v last="$last" '
        /^TEST xp-crc frame=[0-9]+ crc=[0-9A-F]+$/ {
            split($3, f, "=")
            if (f[2] != first + count++ || length($4) != 12) exit 1
            print f[2], $4
        }
        END { if (count != last - first + 1) exit 1 }
    ' "$out/$phase.log" > "$out/$phase.crc" ||
        { printf '%s has missing, repeated or out of order CRC frames\n' "$phase" >&2; exit 1; }
    awk -F '\t' -v first="$first" -v last="$last" '
        /^(AI_[A-Z_]+|SKIRMISH_(CAST|SPELLBOOK|PURCHASE))\t/ && $2 >= first && $2 <= last
    ' "$out/$phase.log" > "$out/$phase.ai"
done

result=0
paste "$out/continue.crc" "$out/resume.crc" | awk '
    $1 != $3 || $2 != $4 { if (!different++) print "First CRC mismatch", $0 }
    END { print "Compared " NR " frames. Differences " different + 0; exit (different > 0) }
' || result=1
if cmp -s "$out/continue.ai" "$out/resume.ai"; then
    printf 'Matched %s AI trace entries\n' "$(wc -l < "$out/continue.ai" | tr -d ' ')"
else
    diff -u "$out/continue.ai" "$out/resume.ai" | head -24
    result=1
fi
for phase in continue resume; do
    if ! cmp -s "$out/save.checkpoint" "$out/$phase.checkpoint"; then
        printf 'Checkpoint changed in %s\n' "$phase"
        diff -u "$out/save.checkpoint" "$out/$phase.checkpoint" || :
        result=1
    fi
done
awk -F '\t' -v first="$first" -v a="$3" -v b="$4" '
    $1 == "SKIRMISH_CHECKPOINT" && $2 == first {
        print
        factions[$4]++
        players++
        buildings += $5
        queued += $6
        moving += $7
    }
    $1 == "AI_WAVE" && $2 < first { waves++ }
    $1 == "SKIRMISH_SPELLBOOK" && $2 < first { casts++ }
    END {
        printf "Checkpoint coverage: %d buildings, %d active unit queues, %d moving attackers, %d waves issued, %d spellbook casts\n",
            buildings, queued, moving, waves, casts
        exit (players != 2 || !factions[a] || !factions[b] || !buildings || !queued || moving < 2 || !waves || !casts)
    }
' "$out/save.log" || result=1
if [ "$result" -eq 0 ] && [ "${SKIRMISH_SAVE_KEEP_STATE:-0}" = 0 ]; then
    rm -rf "$out"
fi
exit "$result"
