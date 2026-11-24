check() {
    status=0
    for frame in 250 270 300; do
        if covers "$frame"; then
            awk -v frame="$frame" '
                $1 == "TEST" && $2 == "locomotor-fields" && $3 == frame {
                    print
                    ok = $4 == 1 && $5 < -0.40 && $5 > -0.60 && $6 > 0.95
                }
                END { exit !ok }
            ' "$log" || status=1
        fi
    done
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
