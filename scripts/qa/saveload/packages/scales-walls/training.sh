check() {
    if covers 190; then
        expect 'TEST scales-walls-destination frame=190 name=ScalesWallsUnit wall=1'
        expect 'TEST scales-walls-destination frame=190 name=ScalesWallsControl wall=1'
    fi
    if covers 220; then
        expect 'TEST scales-walls-capability name=ScalesWallsUnit value=1'
        expect 'TEST scales-walls-capability name=ScalesWallsControl value=0'
    fi
    if covers 244; then
        awk '
            $1 == "TEST" && $2 == "scales-walls" && $3 == "frame=244" && $4 == "name=ScalesWallsUnit" {
                split($5, z, "="); if (z[2] + 0 > 65 && z[2] + 0 < 110) midclimb = 1
            }
            END { if (!midclimb) { print "Wall scaler was not mid-climb at save frame 244"; exit 1 } }
        ' "$log"
    fi
    if covers 300; then
        awk '
            $1 == "TEST" && $2 == "scales-walls" && $3 == "frame=300" {
                split($5, z, "="); split($6, layer, "=")
                if ($4 == "name=ScalesWallsUnit" && layer[2] == 15) climbed = 1
                if ($4 == "name=ScalesWallsControl" && layer[2] == 1) blocked = 1
            }
            $1 == "TEST" && $2 == "scales-walls" && $4 == "name=ScalesWallsCrosser" {
                split($6, layer, "=")
                if (layer[2] == 15) crosser_on_wall = 1
            }
            END { if (!climbed || !blocked || !crosser_on_wall) { print "Wall scaling, control, or single-order crosser failed"; exit 1 } }
        ' "$log"
    fi
    if covers 600; then
        awk '
            $1 == "TEST" && $2 == "scales-walls" && $3 == "frame=600" {
                split($8, y, "="); split($6, layer, "=")
                if ($4 == "name=ScalesWallsUnit" && y[2] + 0 < 2867 && layer[2] == 1) crossed = 1
                if ($4 == "name=ScalesWallsCrosser" && y[2] + 0 < 2867 && layer[2] == 1) single_crossed = 1
            }
            END { if (!crossed || !single_crossed) { print "Wall scaler or single-order crosser did not cross the wall"; exit 1 } }
        ' "$log"
    fi
}
