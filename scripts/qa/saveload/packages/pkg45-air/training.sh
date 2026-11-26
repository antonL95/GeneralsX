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
            if (station == "approach") {
                passed = value["swoop"] == 1 && speed > 0 && value["vz"] > 0 && value["shot"] == 0
            } else if (station == "follow-through") {
                passed = value["swoop"] == 0 && speed > 0 && value["shot"] > 0 && value["height"] > value["preferred"]
            } else if (station == "straight") {
                passed = value["swoop"] == 0 && value["vx"] > 0 && value["vy"] ^ 2 < value["vx"] ^ 2
            } else if (station == "turn") {
                passed = value["swoop"] == 0 && value["vy"] > 0 && value["yaw"] > 0
            } else {
                passed = value["swoop"] == 0 && speed == 0
            }
            print
        }
        END { exit !passed }
    ' "$log"
}

check() {
    check_flight_sample 197 approach
    check_flight_sample 244 approach
    check_flight_sample 245 approach
    check_flight_sample 253 approach
    check_flight_sample 299 follow-through
    check_flight_sample 328 straight
    check_flight_sample 346 turn
    check_flight_sample 357 stop
}

compare() {
    sed -n '/^TEST air-swoop .* actor=Pkg45Eagle /p' "$out/resume.log" > "$out/pkg45-resume.txt"
    sed -n '/^TEST air-swoop .* actor=Pkg45Eagle /p' "$out/continue.log" |
        awk -v frame="$save_frame" '{ split($3, sample, "="); if (sample[2] >= frame) print }' > "$out/pkg45-continue.txt"
    diff -u "$out/pkg45-continue.txt" "$out/pkg45-resume.txt"
}
