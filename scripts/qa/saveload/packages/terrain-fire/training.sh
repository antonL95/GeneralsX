check() {
    if covers 193; then
        grep -Eq '^TEST terrain fire frame=193 burning=0 fuel=0 rate=0 health=' "$log" || return 1
    fi
    if covers 243; then
        grep -Eq '^TEST terrain fire frame=243 burning=[1-9][0-9]* fuel=[1-9][0-9]* rate=[1-9][0-9]* health=' "$log" || return 1
    fi
    if covers 245; then
        grep -Eq '^TEST terrain fire frame=245 burning=[1-9][0-9]* fuel=[1-9][0-9]* rate=[1-9][0-9]* health=' "$log" || return 1
    fi
    if covers 1000; then
        grep -Eq '^TEST terrain fire frame=1000 burning=0 fuel=0 rate=0 health=' "$log" || return 1
    fi
    if covers 193 && covers 243; then
        awk '
            /^TEST terrain fire frame=193 / { split($8, field, "="); initial=field[2]; seen++ }
            /^TEST terrain fire frame=243 / { split($8, field, "="); active=field[2]; seen++ }
            END { exit (seen != 2 || initial != 2000 || active >= initial || active <= 0) }
        ' "$log" || return 1
    fi
    if covers 1037; then
        grep -Eq '^TEST terrain fire frame=1037 burning=0 fuel=70 rate=0 .* x=2595 y=535$' "$log" || return 1
        grep -Eq '^TEST terrain fire frame=1037 burning=0 fuel=70 rate=0 .* x=2605 y=535$' "$log" || return 1
        grep -Eq '^TEST terrain fire frame=1037 burning=0 fuel=70 rate=20 .* x=2615 y=535$' "$log" || return 1
    fi
}

compare() {
    sed -n '/^TEST terrain fire frame=\(245\|275\|330\|1000\|1037\) /p' "$out/continue.log" > "$out/terrain-fire-continue.txt"
    sed -n '/^TEST terrain fire frame=\(245\|275\|330\|1000\|1037\) /p' "$out/resume.log" > "$out/terrain-fire-resume.txt"
    diff -u "$out/terrain-fire-continue.txt" "$out/terrain-fire-resume.txt"
}
