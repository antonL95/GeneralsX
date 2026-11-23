frame_offset() {
    if [ "$end" -ge 1451 ]; then
        printf '1117\n'
    else
        printf '0\n'
    fi
}

prepare() {
    offset=$(frame_offset)
    awk -v offset="$offset" -v end="$end" '
        $2 == "create" && offset { $1 = 220 }
        { $1 += offset; if ($1 <= end) print }
    ' "$package_dir/commands.txt"

    frame=$((239 + offset))
    while [ "$frame" -le "$((332 + offset))" ]; do
        for layout in 0 1; do
            for horde in Guard Archers Knights; do
                printf '%s steering-reaction Player_1 GroupStart%s %s\n' "$frame" "$horde" "$layout"
            done
        done
        if [ "$frame" -eq "$((248 + offset))" ]; then
            for layout in 0 1; do
                for actor in Guard Archers Knights Hero; do
                    printf '%s steering-path Player_1 GroupStart%s %s\n' "$frame" "$actor" "$layout"
                done
            done
        fi
        frame=$((frame + 1))
    done
}

check() {
    offset=$(frame_offset)
    if ! covers "$((332 + offset))" || ! covers "$((230 + offset))"; then
        return 0
    fi
    awk -v track_frame="$((230 + offset))" -v order_frame="$((240 + offset))" -f "$package_dir/measure.awk" "$log" | awk '
        $1 == "cohort" && $3 == "actors=44" {
            ++cohorts
        }
        $1 == "actor" && $5 == "anchor" &&
            (($2 == 0 && $3 == "GroupStartArchers") || ($2 == 1 && $3 == "GroupStartKnights")) {
            limit = $2 == 0 ? 10 : 30
            split($7, first, "=")
            if (first[2] !~ /^[0-9]+$/ || first[2] > limit) {
                printf "Layout %s: %s first eastward step at +%s, expected by +%d\n", $2, $3, first[2], limit
                failed = 1
            }
            ++starts
        }
        $1 == "frame" && $3 == 30 {
            split($6, error, "=")
            split($8, width, "=")
            limit = $2 == 0 ? 60 : 75
            if (error[2] > limit) {
                printf "Layout %s: mean heading error at +30 is %s degrees, expected at most %d\n", $2, error[2], limit
                failed = 1
            }
            if (width[2] > 250) {
                printf "Layout %s: lateral spread at +30 is %s, expected at most 250\n", $2, width[2]
                failed = 1
            }
            ++heading
        }
        END {
            if (cohorts != 2 || starts != 2 || heading != 2)
                print "Incomplete group start trace"
            exit (failed || cohorts != 2 || starts != 2 || heading != 2)
        }
    '
}
