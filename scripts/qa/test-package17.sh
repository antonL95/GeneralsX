#!/bin/sh
set -eu

build=${1:?build directory required}
qa=$(cd "$(dirname "$0")" && pwd)
export PKGS1_TRAINING_EXTRA_INI="$qa/saveload/package17.ini"
export PKGS1_TRAINING_EXTRA_COMMANDS="$qa/saveload/package17.txt"
result=$(mktemp)
trap 'rm -f "$result"' EXIT
if ! sh "$qa/test-save-load-crc.sh" "$build" training 244 181 > "$result" 2>&1; then
    cat "$result"
    exit 1
fi
cat "$result"
session=$(sed -n '1p' "$result")
log="$session/continue.log"
for sample in '225 0 0.60' '234 1 0.80' '245 1 0.80' '405 0 0.60'; do
    read -r frame flank damage <<EOF
$sample
EOF
    grep -E "^TEST weapon-fx frame=$frame weapon=Pkg17Flank .* flank=$flank$" "$log"
    grep -E "^TEST damage frame=$frame weapon=Pkg17Flank .* dealt=$damage " "$log"
done

grep -E '^TEST pkg07 horde frame=190 name=AmrothianGuardsmenHorde members=15 .* facing=0\.000 stance=1 ' "$log"
grep -E '^TEST pkg07 horde frame=190 name=AmrothianGuardsmenHorde members=15 .* facing=0\.000 stance=3 ' "$log"
grep -E '^TEST frame 291: kill object [0-9]+ \(MordorWorker\)$' "$log"
for phase in continue resume; do
    grep -E '^TEST pkg17 (formation|member) ' "$session/$phase.log" |
        awk '{ split($4, frame, "="); if (frame[2] >= 244) print }' > "$session/formation-$phase.txt"
done
diff -u "$session/formation-continue.txt" "$session/formation-resume.txt"
grep '^TEST pkg17 formation ' "$session/formation-continue.txt"
awk '
    function value(field) {
        sub(/^[^=]*=/, "", field)
        return field
    }
    /^TEST horde spawn 174 AmrothianGuardsmenHorde / {
        if (++guards == 2)
            holdGround = $6
    }
    /^TEST horde spawn 174 DolAmrothSquiresHorde / { squires = $6 }
    /^TEST pkg17 formation / {
        frame = value($4) + 0
        if (frame == 244)
            savedBacking += value($8)
        if (value($5) != holdGround)
            next
        if (frame == 244)
            heldPosition = $10 " " $11
        if (frame >= 275 && heldPosition == ($10 " " $11) && value($8) + 0 == 0 && value($9) + 0 > 0)
            reengaged = 1
    }
    /^TEST pkg17 member / {
        frame = value($4) + 0
        id = value($6)
        backing = value($7) + 0
        attacking = value($8) + 0
        target = value($9) + 0
        if (value($5) == holdGround) {
            if (frame == 230 && attacking && target)
                initialTarget[id] = target
            if (backing) {
                if (!firstBacking[id])
                    firstBacking[id] = frame
                backingAt[frame] = 1
            } else if (attacking && target) {
                if (frame > 230 && initialTarget[id] == target && !firstBacking[id])
                    keptTarget[frame, id] = 1
                if (firstBacking[id] && !recoveredAt[id])
                    recoveredAt[id] = frame
            }
        }
        if (value($5) == squires) {
            x = value($12) + 0
            fearX = value($13) + 0
            if (frame == 235 && !backing && fearX > x)
                beforeCrossing[id] = x
            if (backing && value($11) + 0 < -0.1 && (id in beforeCrossing)) {
                if (fearX >= 0 && fearX < x && value($10) + 0 < -0.5 && x < beforeCrossing[id] - 1)
                    facedMovingSource = 1
                if (fearX < 0 && value($10) + 0 > 0.5)
                    facedFallback = 1
            }
        }
    }
    END {
        for (sample in keptTarget) {
            split(sample, member, SUBSEP)
            if ((member[1] in backingAt) && !firstBacking[member[2]])
                fightingDuringBacking = 1
        }
        for (id in recoveredAt)
            for (later in firstBacking)
                if (firstBacking[later] > recoveredAt[id])
                    recoveredBeforeLaterBackup = 1
        printf "Backing at save %d, kept target %d, recovered before later backstep %d, reengaged %d, tracked scarer %d, fallback %d\n",
            savedBacking, fightingDuringBacking, recoveredBeforeLaterBackup, reengaged, facedMovingSource, facedFallback
        exit !(savedBacking && fightingDuringBacking && recoveredBeforeLaterBackup && reengaged && facedMovingSource && facedFallback)
    }
' "$log"
