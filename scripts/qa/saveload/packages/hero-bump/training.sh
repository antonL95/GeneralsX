frame_offset() {
    if [ "$end" -ge 1451 ]; then
        printf '1000\n'
    else
        printf '0\n'
    fi
}

prepare() {
    offset=$(frame_offset)
    awk -v offset="$offset" -v end="$end" '{ $1 += offset; if ($1 <= end) print }' "$package_dir/commands.txt"
    frame=$((190 + offset))
    while [ "$frame" -le "$((440 + offset))" ] && [ "$frame" -le "$end" ]; do
        printf '%s hero-bump-probe Player_1 HeroBumpMount 0 Player_1:HeroBumpKnights:0\n' "$frame"
        if [ "$frame" -ge "$((264 + offset))" ]; then
            printf '%s hero-bump-probe Player_1 YieldSingleFighter 0 Player_1:YieldSingleRider:0\n' "$frame"
            printf '%s hero-bump-probe Player_1 YieldDoubleFighter 0 Player_1:YieldFirstRider:0\n' "$frame"
            printf '%s hero-bump-probe Player_1 YieldDoubleFighter 0 Player_1:YieldSecondRider:0\n' "$frame"
        fi
        frame=$((frame + 1))
    done
    if [ "$end" -ge "$((441 + offset))" ]; then
        printf '%s destroy Player_1 HeroBumpMount 0\n' "$((441 + offset))"
        printf '%s destroy Player_1 HeroBumpKnights 0\n' "$((441 + offset))"
    fi
}

check() {
    offset=$(frame_offset)
    covers "$((440 + offset))" || return 0
    checkpoint=0
    if [ "$save_frame" -eq "$((270 + offset))" ]; then
        checkpoint=$save_frame
    fi
    awk -v checkpoint="$checkpoint" '
        $1 == "TEST" && $2 == "hero-bump" && $4 == "HeroBumpMount" {
            if (FNR == NR) {
                if (!contact && $12 != "blocked=0" && $16 ~ /^loco=HeroHorseLocomotor:/)
                    contact = $3
                next
            }
            split($8, position, "[=,]")
            dx = position[2] - 2335
            dy = position[3] - 1340
            if ($3 == checkpoint && $18 == "yield=1")
                saved_yield = 1
            if (contact && $3 >= contact && dx*dx + dy*dy <= 100 && !arrival)
                arrival = $3
        }
        END {
            if (checkpoint && !saved_yield) {
                printf "Mounted hero was outside the temporary yield at frame %d\n", checkpoint
                exit 1
            }
            if (!contact || !arrival || arrival <= checkpoint || arrival - contact > 190) {
                printf "Mounted horde recovery: contact %s, arrival %s within 190 frames required\n",
                    contact ? contact : "missing", arrival ? arrival : "missing"
                exit 1
            }
            printf "Mounted horde recovery: contact %d, arrival %d (%d frames)\n",
                contact, arrival, arrival - contact
        }
    ' "$out/continue.log" "$log" || return 1

    awk -v checkpoint="$checkpoint" -v first="$first" -v earliest="$((270 + offset))" '
        BEGIN {
            single = "YieldSingleFighter"
            double_yield = "YieldDoubleFighter"
            single_rider = "Player_1:YieldSingleRider:0"
            first_rider = "Player_1:YieldFirstRider:0"
            second_rider = "Player_1:YieldSecondRider:0"
        }
        $1 == "TEST" && $2 == "hero-bump-yield" {
            if (FNR == NR && $6 == "waiting=1" && $7 == "active=0" && $8 == "accepted=1" && $9 == "path=1")
                started[$4, $5] = 1
            if ((FNR != NR || $3 < first) && $4 == double_yield && $5 == second_rider &&
                $7 == "active=1" && $8 == "accepted=0" && $9 == "path=0")
                reset = $3
            next
        }
        FNR == NR {next}
        $1 == "TEST" && $2 == "hero-bump" && ($4 == single || $4 == double_yield) {
            if ($3 == checkpoint && $18 == "yield=1" && $10 != "end=0.000,0.000")
                saved[$4, $6] = 1
            if ($4 != single || $3 <= earliest || $18 != "yield=0" || arrival)
                next
            split($8, position, "[=,]")
            dx = position[2] - 1365
            dy = position[3] - 185
            if (dx*dx + dy*dy <= 144)
                arrival = $3
        }
        END {
            if (!started[single, single_rider] || !started[double_yield, first_rider]) {
                print "Soldier yield did not start with a pending move path"
                failed = 1
            }
            if (!reset) {
                print "Second rider did not reset the active yield and clear its path"
                failed = 1
            }
            if (checkpoint && (!saved[single, single_rider] || !saved[double_yield, first_rider])) {
                printf "Soldier yield or requester missing at checkpoint %d\n", checkpoint
                failed = 1
            }
            if (!arrival) {
                print "Soldier move did not recover after the single yield"
                failed = 1
            }
            if (!failed)
                printf "Soldier move recovered at frame %d. Second yield reset at frame %d.\n", arrival, reset
            exit failed
        }
    ' "$out/continue.log" "$log"
}
