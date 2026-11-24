check() {
    status=0
    for frame in 250 270 300; do
        if covers "$frame"; then
            awk -v frame="$frame" '
                $1 == "TEST" && $2 == "locomotor-fields" && $3 == frame && $4 == "P33FieldsHorde" {
                    print
                    ok = $6 == 1 && $7 < -0.40 && $7 > -0.60 && $9 > 0.95
                }
                END { exit !ok }
            ' "$log" || status=1
        fi
    done
    if covers 270 && covers 300; then
        awk -v first="$first" '
            function near(value, expected, tolerance) {
                return value >= expected - tolerance && value <= expected + tolerance
            }
            function sampled(frame) {
                return frame == 270 || frame == 300 || (first <= 250 && (frame == 250 || frame == 259))
            }
            $1 == "TEST" && $2 == "locomotor-fields" && $4 != "P33FieldsHorde" &&
                (sampled($3) || ($3 == 239 && $4 == "P33FieldsZeroHorde")) {
                print
                actor = $4 == "P33FieldsZeroHorde" ? "zero" : ($5 == 0 ? "cardinal" : "diagonal")
                expected = actor == "zero" ? 0 : (actor == "cardinal" ? -0.0011 : -0.0011 / sqrt(2))
                expectedY = actor == "diagonal" ? expected : 0
                key = actor SUBSEP $3
                valid[key] = $6 == (actor != "zero") && near($7, expected, 0.00005) && near($8, expectedY, 0.00005)
                px[key] = $10
                py[key] = $11
            }
            function step(actor, start, end, dx, dy, tolerance) {
                return valid[actor, start] && valid[actor, end] &&
                    near(px[actor, end] - px[actor, start], dx * (end - start), tolerance) &&
                    near(py[actor, end] - py[actor, start], dy * (end - start), tolerance)
            }
            END {
                diagonal = -0.0011 / sqrt(2)
                ok = step("cardinal", 270, 300, -0.0011, 0, 0.004) &&
                    step("diagonal", 270, 300, diagonal, diagonal, 0.004) &&
                    step("zero", 270, 300, 0, 0, 0)
                if (first <= 250)
                    ok = ok && step("cardinal", 250, 259, -0.0011, 0, 0.0025) &&
                        step("diagonal", 250, 259, diagonal, diagonal, 0.0025) &&
                        step("zero", 250, 259, 0, 0, 0) && step("zero", 259, 270, 0, 0, 0)
                if (first <= 239)
                    ok = ok && step("zero", 239, 250, 0, 0, 0)
                exit !ok
            }
        ' "$log" || status=1
    fi
    if covers 395; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 395 {
                print
                split(substr($5, 5), pos, ",")
                if ($4 == "P33FieldsForm")
                    formation = pos[1] > 1720 && pos[1] < 1740 && pos[2] < 2655
                if ($4 == "P33FieldsSolo")
                    independent = pos[1] > 1720 && pos[1] < 1740 && pos[2] > 2660 && pos[2] < 2675
            }
            END { exit !(formation && independent) }
        ' "$log" || status=1
    fi
    if covers 435; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 435 {
                print
                speed = substr($9, 10) + 0
                if ($4 == "P33FieldsSolo")
                    fast = speed > 1.5 && speed < 2.2
                if ($4 == "P33FieldsSlow")
                    slow = speed > 0.8 && speed < 1.2
            }
            END { exit !(fast && slow) }
        ' "$log" || status=1
    fi
    return "$status"
}
