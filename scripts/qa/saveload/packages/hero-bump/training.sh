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
    ' "$out/continue.log" "$log"
}
