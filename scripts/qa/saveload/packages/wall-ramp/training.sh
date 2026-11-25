check() {
    if covers 181; then
        expect 'TEST wall-ramp frame=181 wall=1 ramp1=1 ramp2=1 vertices=6,6,6 faces=2,2,2 heights=44.29,68.13,0.00'
        expect 'TEST wall-ramp-shape frame=181 polygon=4 ramp1=1 ramp2=1'
        expect 'TEST wall-ramp-cell frame=181 inside=1 outside=0 height=44.29'
        expect 'TEST wall-ramp-links frame=181 first=1 second=1'
    fi
    if covers 186; then
        expect 'TEST wall-ramp-cell frame=186 inside=0 outside=0 height=-21.99'
        expect 'TEST wall-ramp-links frame=186 first=0 second=1'
        expect 'TEST wall-ramp-footprint upgraded different=1'
    fi
    if covers 191; then
        expect 'TEST wall-ramp-footprint reset equal=1'
    fi
    if covers 220; then
        awk '
            $1 == "TEST" && $2 == "wall-ramp-unit" && $3 == "frame=220" && $4 == "name=WallRampClimber" {
                split($7, z, "="); split($8, layer, "=")
                if (z[2] > 70 && layer[2] > 1 && layer[2] < 15) climbing = 1
            }
            END { if (!climbing) { print "Climber was not on a ramp at the save frame"; exit 1 } }
        ' "$log"
    fi
    if covers 244; then
        awk '
            $1 == "TEST" && $2 == "wall-ramp-unit" && $3 == "frame=244" && $4 == "name=WallRampClimber" {
                split($7, z, "="); split($8, layer, "=")
                if (z[2] > 95 && layer[2] > 1) supported = 1
            }
            END { if (!supported) { print "Climber fell at the ramp and wall join"; exit 1 } }
        ' "$log"
    fi
    if covers 245; then
        awk '
            $1 == "TEST" && $2 == "wall-ramp-pinched" && $3 == "frame=245" {
                split($5, own, "="); split($6, stale, "=")
                if (own[2] > 0) stamped = 1
                if (stale[2] > 0) old = 1
            }
            END { if (!stamped || old) { print "Wall footprint left stale pinched cells"; exit 1 } }
        ' "$log"
    fi
    if covers 300; then
        awk '
            $1 == "TEST" && $2 == "wall-ramp-unit" && $3 == "frame=300" && $4 == "name=WallRampClimber" {
                split($7, z, "="); split($8, layer, "=")
                if (z[2] > 120 && layer[2] == 15) reached = 1
            }
            END { if (!reached) { print "Climber did not reach the wall top"; exit 1 } }
        ' "$log"
    fi
    if covers 500; then
        awk '
            $1 == "TEST" && $2 == "track" && $5 == "WallRampBlocker" { blocker[$4] = 1 }
            $1 == "TEST" && $2 == "pos" && ($4 in blocker) && $5 == 0 &&
                $6 > 2363 && $6 < 2517 && $7 > 1398 && $7 < 1464 { crossed = 1 }
            END { if (crossed) { print "Ground unit crossed the wall bounds"; exit 1 } }
        ' "$log"
    fi
}
