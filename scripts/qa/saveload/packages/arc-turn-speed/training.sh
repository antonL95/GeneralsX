check() {
    if covers 247; then
        awk '
            $1 == "TEST" && $4 == "ArcTurnSpeedSubject" && $2 == "corner-speed" {
                split($5, scalar, "=")
                split($6, planar, "=")
                split(planar[2], velocity, ",")
                separated = scalar[2] <= 1 && velocity[1] ^ 2 + velocity[2] ^ 2 > 1
                print
            }
            $1 == "TEST" && $4 == "ArcTurnSpeedSubject" && $2 == "steering" {
                if (separated && /left=1 right=0 leftHS=0 rightHS=0/) found = 1
                sub(/ raw:.*/, "")
                print
            }
            END {
                if (!found) print "Expected a normal left turn while only the planar speed exceeds half maximum."
                exit !found
            }
        ' "$log" || return 1
    fi
}
