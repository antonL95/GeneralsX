prepare() {
    seq 221 429 | awk -v end="$end" '$1 <= end {
        print $1, "maggot-dogs-trace Player_1 ShireFarmerMaggot 0 PlyrCreeps:MaggotDogsTarget:0"
        print $1, "maggot-dogs-trace Player_1 ShireFarmerMaggot 1"
    }'
}

check() {
    if ! covers 429; then
        return 0
    fi
    awk '
        BEGIN { pi = atan2(0, -1) }
        $1 == "TEST" && $2 == "maggot-dog" && $3 >= 252 {
            frame = $3
            pack = $4
            dog = $5
            if (pack == 0) {
                if ($15 && frame <= 402)
                    reached[dog] = 1
                if (frame == 429)
                    attackDogs[dog] = 1
            } else {
                if (frame == 429)
                    idleDogs[dog] = 1
                dx = $6 - 500
                dy = $7 - 1000
                if ($14 || dx * dx + dy * dy > 75 * 75)
                    stray[dog] = frame
                dx = $6 - $9
                dy = $7 - $10
                nearGoal = dx * dx + dy * dy <= 12 * 12
                if (nearGoal)
                    guardArrived[dog] = 1
                angle = atan2(dy, dx)
                goal = $9 "," $10
                if (!nearGoal && guardGoal[dog] == goal) {
                    turn = angle - bearing[dog]
                    if (turn > pi)
                        turn -= 2 * pi
                    if (turn < -pi)
                        turn += 2 * pi
                    sweep[dog] += turn
                    if (sweep[dog] > 2 * pi || sweep[dog] < -2 * pi)
                        orbit[dog] = frame
                } else {
                    guardGoal[dog] = goal
                    sweep[dog] = 0
                }
                bearing[dog] = angle
            }
        }
        END {
            for (dog in attackDogs) {
                attackers++
                if (!reached[dog]) {
                    print "maggot dogs: dog", dog, "did not enter melee range by frame 402"
                    failed = 1
                }
            }
            for (dog in idleDogs) {
                idlers++
                if (!guardArrived[dog] || stray[dog] || orbit[dog]) {
                    print "maggot dogs: untargeted dog", dog, "guard arrival", guardArrived[dog] + 0, "stray frame", stray[dog] + 0, "orbit frame", orbit[dog] + 0
                    failed = 1
                }
            }
            if (attackers != 3 || idlers != 3) {
                print "maggot dogs: expected three hounds per Maggot, saw", attackers + 0, idlers + 0, "at frame 429"
                failed = 1
            }
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        awk -v start="$save_frame" '$1 == "TEST" && $2 == "maggot-dog" && $3 >= start' \
            "$out/$phase.log" > "$out/maggot-dogs-$phase.txt"
    done
    diff -u "$out/maggot-dogs-continue.txt" "$out/maggot-dogs-resume.txt"
}
