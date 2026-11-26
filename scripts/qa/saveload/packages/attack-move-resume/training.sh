check() {
    if covers 204; then
        awk '$1 == "TEST" && $2 == "attack-move-resume" && $3 == 204 && $6 == 1 { found = 1 }
            END { if (!found) print "attack-move-resume: expected a live enemy at frame 204"; exit !found }' "$log" || return 1
    fi
    covers 690 || return 0
    awk '
        $1 == "TEST" && $2 == "attack-move-resume" {
            dx = $4 - 1650
            dy = $5 - 605
            shortOfGoal = dx * dx + dy * dy > 20 * 20
            if ($3 < 690 && $6 == 0 && shortOfGoal) clearedBeforeArrival = 1
            if ($3 != 690) next
            seen++
            if ($6 != 0) {
                print "attack-move-resume: enemy still alive at frame 690"
                failed = 1
            }
            if (shortOfGoal) {
                printf "attack-move-resume: frame 690 anchor at (%s, %s), expected within 20 of (1650, 605)\n", $4, $5
                failed = 1
            }
        }
        END {
            if (seen != 1) print "attack-move-resume: expected one frame 690 probe"
            if (!clearedBeforeArrival) print "attack-move-resume: expected the enemy gone before the anchor reached its destination"
            exit (seen != 1 || failed || !clearedBeforeArrival)
        }
    ' "$log"
}
