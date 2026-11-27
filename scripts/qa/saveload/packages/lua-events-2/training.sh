state_matches() {
    if covers "$1"; then
        grep -E "^TEST lua events frame=$1 $2" "$log" || return 1
    fi
}

moved_west() {
    if covers "$1" && covers "$2"; then
        awk -v before="$1" -v after="$2" '
            $1 == "TEST" && $2 == "lua" && $3 == "events" {
                split($4, frame, "=")
                split($10, x, "=")
                if (frame[2] == before) {
                    start = x[2]
                    found_start = 1
                }
                if (frame[2] == after) {
                    end = x[2]
                    found_end = 1
                }
            }
            END { exit !(found_start && found_end && end < start - 1) }
        ' "$log" || return 1
    fi
}

check() {
    for frame in 403 583 736; do
        state_matches "$frame" 'chant=0 ' || return 1
    done
    for frame in 493 673 734; do
        state_matches "$frame" 'chant=1 ' || return 1
    done
    state_matches 674 'chant=0 .* health=10000\.00$' || return 1
    state_matches 679 'chant=0 enrage=0 ' || return 1
    state_matches 681 'chant=0 enrage=1 ' || return 1
    state_matches 695 'chant=0 enrage=0 fear=0 terror=0 fleeing=0 ' || return 1
    state_matches 697 'chant=0 enrage=0 fear=1 terror=0 fleeing=1 ' || return 1
    state_matches 730 'chant=0 enrage=0 fear=1 terror=0 fleeing=1 ' || return 1
    state_matches 731 'chant=0 enrage=0 fear=0 terror=0 fleeing=0 ' || return 1
    moved_west 695 730 || return 1
    state_matches 742 'chant=0 enrage=0 fear=0 terror=0 fleeing=0 ' || return 1
    state_matches 793 'chant=0 enrage=0 fear=1 terror=1 fleeing=1 ' || return 1
    state_matches 794 'chant=0 enrage=0 fear=0 terror=0 fleeing=0 ' || return 1
    moved_west 742 793 || return 1
    state_matches 902 '.* health=10000\.00$' || return 1
    if covers 1080; then
        awk '$1 == "TEST" && $2 == "lua" && $3 == "events" && $4 == "frame=1080" {
            split($12, health, "=")
            print
            damaged = health[1] == "health" && health[2] > 0 && health[2] < 10000
        }
        END { exit !damaged }' "$log" || return 1
    fi
}

compare() {
    for run in continue resume; do
        grep '^TEST lua events frame=' "$out/$run.log" > "$out/lua-events-$run.txt" || return 1
    done
    diff -u "$out/lua-events-continue.txt" "$out/lua-events-resume.txt"
}
