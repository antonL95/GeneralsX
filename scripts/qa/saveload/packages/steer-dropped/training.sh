check() {
    covers 240 && covers 270 || return 0
    awk '
        $1 == "TEST" && $2 == "steer-dropped" && $3 == 240 {
            x[$4] = $5; y[$4] = $6; seen++
        }
        $1 == "TEST" && $2 == "steer-dropped" && $3 == 270 && $4 in x {
            dx = $5 - x[$4]; dy = $6 - y[$4]
            checked++
            if (dx * dx + dy * dy < 0.01) {
                print "Steering horde member failed to move after braking:", $4
                failed = 1
            }
        }
        END {
            if (!seen || checked != seen) { print "Missing wheeled horde member samples"; failed = 1 }
            if (failed) exit 1
        }
    ' "$log"
}
