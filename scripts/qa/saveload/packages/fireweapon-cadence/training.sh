check() {
    for sample in '225 zero 1000' '225 delayed 1000' '225 death 1000' \
        '244 zero 970' '244 delayed 990' '244 death 970' \
        '255 zero 960' '255 delayed 990' '255 death 960' \
        '285 zero 910' '285 delayed 970' '285 death 910'; do
        if covers "${sample%% *}"; then
            expect "TEST fireweapon-cadence health $sample" || return 1
        fi
    done

    awk -v first="$first" -v end="$end" '
        BEGIN {
            for (frame = 231; frame < 285; frame += 6) {
                if (frame >= first && frame <= end) {
                    expected["CadenceFixtureZero", frame] = 1
                    expected["CadenceFixtureDeathSpawn", frame] = 1
                }
            }
            for (frame = 237; frame < 285; frame += 18)
                if (frame >= first && frame <= end)
                    expected["CadenceFixtureDelayed", frame] = 1
        }
        /^TEST timed weapon / && /\(CadenceFixture/ {
            split($4, stamp, "=")
            split($5, source, /[()]/)
            key = source[2] SUBSEP stamp[2]
            if (!expected[key] || seen[key]++) {
                if (!failed)
                    print "Unexpected cadence shot: " $0 > "/dev/stderr"
                failed = 1
            }
        }
        END {
            for (key in expected)
                if (!seen[key]) {
                    split(key, shot, SUBSEP)
                    print "Missing cadence shot: " shot[1] " at " shot[2] > "/dev/stderr"
                    failed = 1
                }
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '
            /^TEST fireweapon-cadence health / && $4 >= first { print }
            /^TEST timed weapon / && /\(CadenceFixture/ {
                split($4, stamp, "=")
                if (stamp[2] >= first)
                    print
            }
        ' "$out/$phase.log" > "$out/fireweapon-cadence-$phase.txt"
    done
    diff -u "$out/fireweapon-cadence-continue.txt" "$out/fireweapon-cadence-resume.txt"
}
