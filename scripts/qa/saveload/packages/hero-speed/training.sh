check() {
    awk -v first="$first" -v end="$end" '
        function require_sample(frame, hero, label, rank, speed, toggle, key) {
            if (frame < first || frame > end)
                return
            key = frame SUBSEP hero
            if (!(key in seen) || mode[key] != label || level[key] != rank ||
                (speed >= 0 && abs(actual[key] - speed) > 0.05) || switched[key] != toggle ||
                abs(base[key] - 55) > 0.01 || set[key] != 0 || group[key] || horde[key] ||
                (label != "sword" && abs(maximum[key] - 55) > 0.01)) {
                printf "Unexpected %s speed sample at frame %d: %s\n", hero, frame, seen[key]
                failed = 1
            }
        }
        function require_travel(start, stop, hero, before, after, dx, dy, speed) {
            if (start < first || stop > end)
                return
            before = start SUBSEP hero
            after = stop SUBSEP hero
            dx = x[after] - x[before]
            dy = y[after] - y[before]
            speed = sqrt(dx * dx + dy * dy) * 30 / (stop - start)
            if (dx <= 0 || abs(speed - 55) > 0.1) {
                printf "%s traveled %.3f units/sec on frames %d to %d; expected 55\n", hero, speed, start, stop
                failed = 1
            }
        }
        function abs(value) { return value < 0 ? -value : value }
        $1 == "TEST" && $2 == "hero-speed" {
            key = $3 SUBSEP $4
            seen[key] = $0
            mode[key] = $5
            level[key] = substr($6, 6) + 0
            split(substr($7, 5), position, ",")
            x[key] = position[1]
            y[key] = position[2]
            actual[key] = substr($9, 8) + 0
            maximum[key] = substr($10, 5) + 0
            base[key] = substr($11, 6) + 0
            set[key] = substr($14, 5) + 0
            group[key] = substr($16, 12) + 0
            horde[key] = substr($17, 7) + 0
            switched[key] = substr($19, 8) + 0
        }
        END {
            heroes[0] = "ArnorHalbarad"
            heroes[1] = "RohanMerry"
            heroes[2] = "ElvenAragorn"
            for (i = 0; i < 3; ++i) {
                label = i == 0 ? "bow" : "comparison"
                for (frame = 235; frame <= 295; frame += 30)
                    require_sample(frame + i, heroes[i], label, 1, 55, 0)
                require_travel(235 + i, 265 + i, heroes[i])
                require_travel(265 + i, 295 + i, heroes[i])
            }
            require_sample(330, "ArnorHalbarad", "bow-high", 5, 55, 0)
            require_sample(371, "ArnorHalbarad", "sword", 5, -1, 1)
            require_sample(401, "ArnorHalbarad", "sword", 5, -1, 1)
            require_sample(420, "ArnorHalbarad", "bow-again", 5, -1, 0)
            require_sample(475, "ArnorHalbarad", "arrival", 5, 0, 0)
            exit failed
        }
    ' "$log"
}

compare() {
    for run in continue resume; do
        awk -v frame="$save_frame" '$1 == "TEST" && $2 == "hero-speed" && $3 >= frame' "$out/$run.log" > "$out/hero-speed-$run.txt"
    done
    cmp "$out/hero-speed-continue.txt" "$out/hero-speed-resume.txt"
}
