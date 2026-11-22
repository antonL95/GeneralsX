check() {
    for entry in 190:0 230:0 243:16 244:16 260:16 290:16 320:1 350:1 380:1 420:1 490:1 536:0; do
        frame=${entry%:*}
        count=${entry#*:}
        if covers "$frame"; then
            expect "TEST army-of-the-dead $frame count $count" || return 1
        fi
    done

    if covers 244; then
        awk '
            function abs(value) { return value < 0 ? -value : value }
            /^TEST army-of-the-dead 244 / && $4 != "count" {
                seen[$4]++
                if ($11 != 0 || $12 != 0 || $10 != 1 || abs($9) > 0.001)
                    failed = 1
                if ($4 == "ElessarOathbreakerFlood") {
                    if ($6 != 0 || $7 != 0 || $13 != 291)
                        failed = 1
                } else {
                    lanes[$7]++
                    if ($6 < -100 || $6 > -35 || $8 != 60)
                        failed = 1
                }
                if ($4 == "FloodKotD" && (abs($6 - (-49.205)) > 0.02 || $7 != 0))
                    failed = 1
            }
            END {
                split("-75.000 -70.000 -50.000 -45.000 -30.000 -25.000 5.000 30.000 35.000 50.000 55.000 65.000 75.000", expected)
                for (i in expected)
                    if (lanes[expected[i]] != 1)
                        failed = 1
                if (failed || lanes["0.000"] != 2 || seen["ElessarOathbreakerFlood"] != 1 ||
                    seen["FloodKotD"] != 1 || seen["FloodOathbreaker"] != 9 || seen["FloodOathbreakerCOM"] != 5) {
                    print "army-of-the-dead: unexpected active flood" > "/dev/stderr"
                    exit 1
                }
            }
        ' "$log" || return 1
    fi

    if covers 260; then
        awk '
            /^TEST army-of-the-dead 260 FloodKotD / {
                seen++
                if ($6 < 26.57 || $6 > 26.63 || $7 != 0 || $10 != 1 || $12 != 0)
                    failed = 1
            }
            END { exit (seen != 1 || failed) }
        ' "$log" || return 1
    fi

    if covers 290; then
        awk '
            /^TEST army-of-the-dead 290 FloodKotD / {
                seen++
                if ($6 != 110 || $7 != 0 || $10 <= 0 || $10 >= 1 || $12 != 1)
                    failed = 1
            }
            END { exit (seen != 1 || failed) }
        ' "$log" || return 1
    fi
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 == "army-of-the-dead" && $3 >= first' \
            "$out/$phase.log" > "$out/army-of-the-dead-$phase.txt"
    done
    diff -u "$out/army-of-the-dead-continue.txt" "$out/army-of-the-dead-resume.txt"
}
