check() {
    for frame in 243 247 300; do
        if covers "$frame"; then
            awk -v frame="$frame" '$1 == "TEST" && $2 == "steering" && $3 == frame && $4 == "AiTuningFixtureMelee" {
                for (node = 1; node <= NF; node++)
                    if ($node == "itinerary:") {
                        split($(node - 1), position, ",");
                        if (frame == 300 && position[1] >= 2600 && position[1] <= 2800) found = 1
                        if (frame != 300 && position[1] >= 2460 && position[1] <= 2490) found = 1
                    }
                if (!found) print
            } END { exit !found }' "$log" || return 1
        fi
    done

    if covers 282; then
        grep -E '^TEST health 282 Player_1 AiTuningFixtureRanged 0 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST health 282 PlyrCreeps AiTuningFixtureRanged 0 .*health=980\.00 ' "$log" || return 1
    fi
    if covers 283; then
        grep -E '^TEST health 283 Player_1 AiTuningFixtureRanged 1 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST health 283 PlyrCreeps AiTuningFixtureRanged 1 .*health=1000\.00 ' "$log" || return 1
    fi
    if covers 294; then
        grep -E '^TEST health 294 Player_1 AiTuningFixtureRanged 2 .*health=980\.00 ' "$log" || return 1
    fi
    if covers 520; then
        grep -E '^TEST health 520 PlyrCreeps AiTuningFixtureRanged 2 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST health 520 PlyrCreeps AiTuningFixtureRanged 3 .*health=1000\.00 ' "$log" || return 1
    fi
}
