check_flight_sample() {
    if ! covers "$1"; then
        return 0
    fi
    awk -v frame="$1" -v station="$2" '
        $1 == "TEST" && $2 == "air-swoop" && $3 == "frame=" frame && $4 == "actor=Pkg45Eagle" {
            for (i = 5; i <= NF; ++i) {
                split($i, field, "=")
                value[field[1]] = field[2]
            }
            speed = sqrt(value["vx"] ^ 2 + value["vy"] ^ 2)
            if (station == "dive") {
                passed = value["swoop"] == 1 && speed > 0 && value["vz"] < 0 && value["vz"] >= -5.401
            } else if (station == "terminal") {
                passed = value["swoop"] == 1 && speed > 0 && value["vz"] == -5.400
            } else if (station == "straight") {
                passed = value["swoop"] == 0 && value["vx"] > 0 && value["vy"] ^ 2 < value["vx"] ^ 2
            } else if (station == "turn") {
                passed = value["swoop"] == 0 && value["vy"] > 0 && value["vx"] ^ 2 < value["vy"] ^ 2
            } else if (station == "stop") {
                passed = value["swoop"] == 0 && speed == 0
            } else {
                passed = value["swoop"] == 0 && speed > 0 && value["vz"] > 0 && value["vz"] < speed * 0.65
                preferred = station == "staging" ? 119 : 121
                passed = passed && value["preferred"] == preferred
            }
            print
        }
        END { exit !passed }
    ' "$log"
}

check() {
    check_flight_sample 197 staging
    check_flight_sample 244 dive
    check_flight_sample 245 dive
    check_flight_sample 253 terminal
    check_flight_sample 299 climb
    check_flight_sample 328 straight
    check_flight_sample 346 turn
    check_flight_sample 357 stop
    if covers 244 && covers 245; then
        awk '
            /^TEST air-swoop frame=(244|245) actor=Pkg45Eagle / {
                for (i = 5; i <= NF; ++i) {
                    split($i, field, "=")
                    if (field[1] == "vz")
                        vz[$3] = field[2]
                }
            }
            END {
                acceleration = vz["frame=244"] - vz["frame=245"]
                exit !(acceleration >= 0.359 && acceleration <= 0.361)
            }
        ' "$log"
    fi
}

compare() {
    sed -n '/^TEST air-swoop .* actor=Pkg45Eagle /p' "$out/resume.log" > "$out/pkg45-resume.txt"
    sed -n '/^TEST air-swoop .* actor=Pkg45Eagle /p' "$out/continue.log" |
        awk -v frame="$save_frame" '{ split($3, sample, "="); if (sample[2] >= frame) print }' > "$out/pkg45-continue.txt"
    diff -u "$out/pkg45-continue.txt" "$out/pkg45-resume.txt"
}
