prepare() {
    seq 205 300 | awk -v end="$end" '$1 <= end {
        print $1, "scales-walls-sample Player_1 WallClimbNoLayerUnit 0"
        print $1, "scales-walls-sample Player_1 WallClimbLayerUnit 0"
    }'
}

check() {
    if covers 194; then
        expect 'TEST scales-walls-obstacle frame=194 name=WallClimbPalisade wall=0'
        expect 'TEST scales-walls-obstacle frame=194 name=WallClimbFortification wall=1'
    fi
    awk -v first="$first" -v end="$end" '
        function value(field) { sub(/^[^=]*=/, "", field); return field + 0 }
        function closeEnough(a, b) { return a - b < 0.02 && b - a < 0.02 }
        $1 == "TEST" && $2 == "scales-walls" && $4 ~ /^name=WallClimb/ {
            frame = value($3)
            name = $4
            z = value($5)
            layer = value($6)
            x = value($7)
            y = value($8)
            speed = value($9)
            if (frame == previousFrame[name] + 1 && speed > 0 &&
                closeEnough(x, previousX[name]) && closeEnough(y, previousY[name])) {
                dz = z - previousZ[name]
                if (closeEnough(dz, 0.6 * speed) && frame <= 244)
                    climbed[name] = 1
                if (closeEnough(dz, -0.6 * speed))
                    descended[name] = 1
            }
            previousFrame[name] = frame
            previousZ[name] = z
            previousX[name] = x
            previousY[name] = y
            if (layer == 15)
                reachedWallLayer[name] = 1
            if (frame == 600 && layer == 1 &&
                ((name == "name=WallClimbNoLayerUnit" && x > 2235) ||
                 (name == "name=WallClimbLayerUnit" && x > 2375)))
                crossed[name] = 1
        }
        END {
            split("name=WallClimbNoLayerUnit name=WallClimbLayerUnit", units)
            for (i in units) {
                name = units[i]
                if (first <= 205 && end >= 244 && !climbed[name]) {
                    print name " did not climb at the retail rate by the checkpoint"
                    failed = 1
                }
                if (first <= 600 && end >= 600 && !crossed[name]) {
                    print name " did not cross the wall and return to the ground by frame 600"
                    failed = 1
                }
                if (first <= 244 && end >= 300 && !descended[name]) {
                    print name " did not descend at the retail rate by frame 300"
                    failed = 1
                }
            }
            if (first <= 244 && end >= 300 &&
                (!reachedWallLayer["name=WallClimbLayerUnit"] || reachedWallLayer["name=WallClimbNoLayerUnit"])) {
                print "The climbers did not use the expected layers at the crest"
                failed = 1
            }
            exit failed
        }
    ' "$log"
}
