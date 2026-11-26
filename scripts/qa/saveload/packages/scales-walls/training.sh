check() {
    if covers 190; then
        expect 'TEST scales-walls-destination frame=190 name=ScalesWallsUnit wall=1'
        expect 'TEST scales-walls-destination frame=190 name=ScalesWallsControl wall=1'
    fi
    if covers 220; then
        expect 'TEST scales-walls-capability name=ScalesWallsUnit value=1'
        expect 'TEST scales-walls-capability name=ScalesWallsControl value=0'
    fi
    if covers 300; then
        awk '
            $1 == "TEST" && $2 == "scales-walls" && $3 == "frame=300" {
                split($5, z, "="); split($6, layer, "=")
                if ($4 == "name=ScalesWallsUnit" && layer[2] == 15) climbed = 1
                if ($4 == "name=ScalesWallsControl" && layer[2] == 1) blocked = 1
            }
            END { if (!climbed || !blocked) { print "Wall scaling or control failed"; exit 1 } }
        ' "$log"
    fi
    if covers 600; then
        awk '
            $1 == "TEST" && $2 == "scales-walls" && $3 == "frame=600" && $4 == "name=ScalesWallsUnit" {
                split($8, y, "="); split($6, layer, "="); if (y[2] < 2867 && layer[2] == 1) crossed = 1
            }
            END { if (!crossed) { print "Wall scaler did not cross the wall"; exit 1 } }
        ' "$log"
    fi
}
