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
for phase in continue resume; do
    grep '^TEST pkg17 formation ' "$session/$phase.log" > "$session/formation-$phase.txt"
done
diff -u "$session/formation-continue.txt" "$session/formation-resume.txt"
cat "$session/formation-continue.txt"
awk '
    /^TEST pkg17 formation / {
        split($4, frame, "=")
        split($5, id, "=")
        split($8, backing, "=")
        split($9, attacking, "=")
        if (frame[2] == 244) {
            anchor[id[2]] = $10
            savedBacking += backing[2]
        }
        if (frame[2] == 275 && backing[2] > 0 && anchor[id[2]] == $10)
            ++backingHordes
        if (frame[2] == 349 && backing[2] == 0 && attacking[2] > 0 && anchor[id[2]] == $10)
            ++reengaged
    }
    END { exit !(savedBacking > 0 && backingHordes == 3 && reengaged > 0) }
' "$session/formation-continue.txt"
