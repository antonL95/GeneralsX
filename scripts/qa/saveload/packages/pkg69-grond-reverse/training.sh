check() {
    status=0
    if covers 235; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && $3 == 235 {
                print
                if ($4 == "P69GrondStopTurn")
                    stop_turn = $6 == 1 && $7 == 1 && $9 == 1 && ($10 * $10 + $11 * $11) < 0.0004
                if ($4 == "P69GrondKeepTurn")
                    keep_turn = $6 == 1 && $7 == 1 && $9 == 1 && $10 < 0 &&
                        ($10 * $10 + $11 * $11) > ($16 + 0.1) * ($16 + 0.1) && $16 == 0.2
            }
            END { exit !(stop_turn && keep_turn) }
        ' "$log" || status=1
    fi
    if covers 237; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && $3 == 237 {
                print
                if ($4 == "P69GrondCrew")
                    crew_on = $8 == 1 && $10 > 0.50 && $15 > 0.95 && $15 < 1.05
                if ($4 == "P69NoCrewPower")
                    nocrew_on = $8 == 1 && $10 > 0.80 && $15 > 0.95 && $15 < 1.05
            }
            END { exit !(crew_on && nocrew_on) }
        ' "$log" || status=1
    fi
    if covers 240; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && $3 == 240 {
                print
                if ($4 == "P69GrondReverse")
                    rev = $6 == 1 && $7 == 1 && $8 == 1 && $9 == 0 && $10 < -0.60 && $12 > 0.99 && $16 == 0.25
                if ($4 == "P69GrondFar")
                    far_start = $6 == 1 && $7 == 1 && $10 < -1.20
                if ($4 == "P69GrondAngle")
                    ang = $6 == 0 && $7 == 0
            }
            END { exit !(rev && far_start && ang) }
        ' "$log" || status=1
    fi
    if covers 242; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && $3 == 242 {
                print
                if ($4 == "P69GrondCrew")
                    crew_off = $8 == 0 && ($10 * $10 + $11 * $11) < 0.0001 && $15 == 0
                if ($4 == "P69NoCrewPower")
                    nocrew_off = $8 == 1 && $10 > 0.80 && $15 > 0.95
            }
            END { exit !(crew_off && nocrew_off) }
        ' "$log" || status=1
    fi
    if covers 260 && covers 280; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && ($3 == 260 || $3 == 280) {
                print
                x[$4, $3] = $13 + 0
                if ($4 == "P69GrondReverse")
                    rev[$3] = $6 == 1 && $7 == 1 && $8 == 1 && $10 < -0.90 && $12 > 0.99
                if ($4 == "P69GrondCrew") {
                    crew[$3] = $8 == 0 && $9 == 1 && ($10 * $10 + $11 * $11) < 0.0001 && $15 == 0
                    facing[$3] = $12 + 0
                    crew_y[$3] = $14 + 0
                }
                if ($4 == "P69NoCrewPower")
                    nocrew[$3] = $8 == 1 && $10 > 0.80 && $15 > 0.95
            }
            END {
                ok = rev[260] && rev[280] && x["P69GrondReverse", 280] < x["P69GrondReverse", 260] - 15 &&
                    crew[260] && crew[280] && x["P69GrondCrew", 280] == x["P69GrondCrew", 260] &&
                    crew_y[280] == crew_y[260] && facing[280] < facing[260] - 0.002 &&
                    nocrew[260] && nocrew[280] && x["P69NoCrewPower", 280] > x["P69NoCrewPower", 260] + 15
                exit !ok
            }
        ' "$log" || status=1
    fi
    if covers 278; then
        awk '
            $1 == "TEST" && $2 == "pkg69" && $3 == 278 {
                print
                if ($4 == "P69GrondFar")
                    far = $6 == 0 && $7 == 0 && $12 < 0.9999 && $13 < 940 && $13 > 910
                if ($4 == "P69GrondNear")
                    near = $6 == 1 && $7 == 1 && $10 < -1.95 && $12 > 0.99 && $13 < 940
            }
            END { exit !(far && near) }
        ' "$log" || status=1
    fi
    return "$status"
}
