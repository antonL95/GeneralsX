prepare() {
    for frame in $(seq 201 420); do
        if [ "$frame" -ge 244 ]; then
            for disposition in Chunk Out Up Flying; do
                printf '%s ocl-intensity-trace Player_1 OclIntensity%s 0\n' "$frame" "$disposition"
            done
        fi
        if [ "$frame" -lt 244 ]; then
            for template in BuildingGen01BitA MordorCatapultRockProjectileBit01; do
                printf '%s ocl-intensity-trace Player_1 %s 0\n' "$frame" "$template"
            done
        fi
    done
}

check() {
    covers 244 || return 0
    awk -v last="$end" '
        function abs(v) { return v < 0 ? -v : v }
        function fail(message) {
            print "ocl-intensity:", message > "/dev/stderr"
            failed = 1
        }
        BEGIN {
            launch = 244
            gravity = -64 / (30 * 30)
            epsilon = 0.002
            split("Chunk Out Up Flying", cases, " ")
        }
        $1 == "TEST" && $2 == "ocl-intensity" && $3 >= launch && $4 ~ /^OclIntensity(Chunk|Out|Up|Flying)$/ {
            frame = $3
            kind = substr($4, length("OclIntensity") + 1)
            height = $7
            vx = $8
            vy = $9
            vz = $10
            ground = $11
            if (frame == launch) {
                launched[kind] = 1
                horizontal = kind == "Up" ? 2 : 4
                if (kind == "Chunk") {
                    if (abs(vx - 0.4) > epsilon || abs(vy) > epsilon || abs(vz - gravity) > epsilon)
                        fail("unexpected chunk launch at frame " frame ": " $0)
                } else if (abs(vx) > horizontal + epsilon || abs(vy) > horizontal + epsilon || (vx == 0 && vy == 0)) {
                    fail("unexpected " kind " horizontal launch at frame " frame ": " $0)
                }
                if (kind == "Out") {
                    if (height > epsilon || abs(vz) > epsilon)
                        fail("outward launch left the ground at frame " frame ": " $0)
                    landing[kind] = launch
                } else {
                    if (height <= epsilon)
                        fail(kind " was grounded at the checkpoint: " $0)
                    if (kind == "Up" || kind == "Flying") {
                        low = kind == "Up" ? 3 : 0.99
                        high = kind == "Up" ? 4 : 3
                        impulse = vz - gravity
                        if (impulse < low - epsilon || impulse > high + epsilon)
                            fail("unexpected " kind " vertical launch at frame " frame ": " $0)
                    }
                }
            } else if (!(kind in landing)) {
                if (previousFrame[kind] != frame - 1)
                    fail("missing " kind " flight sample before frame " frame)
                predicted = previousHeight[kind] + previousVz[kind] + gravity + previousGround[kind] - ground
                if (height <= epsilon) {
                    if (predicted > epsilon)
                        fail(kind " landed early at frame " frame ": " $0)
                    landing[kind] = frame
                } else if (predicted < -epsilon) {
                    fail(kind " missed ground contact at frame " frame ": " $0)
                }
            }
            previousFrame[kind] = frame
            previousHeight[kind] = height
            previousVz[kind] = vz
            previousGround[kind] = ground
        }
        END {
            for (i = 1; i <= 4; ++i) {
                kind = cases[i]
                if (!launched[kind])
                    fail("missing " kind " launch at frame " launch)
                if (last >= 420 && !(kind in landing))
                    fail("missing " kind " first ground contact")
                if (kind in landing)
                    print "ocl-intensity", kind, "first ground contact", landing[kind]
            }
            exit failed
        }
    ' "$log"
}

compare() {
    for run in continue resume; do
        awk -v start="$save_frame" '
            $1 == "TEST" && $2 == "ocl-intensity" && $3 >= start && $4 ~ /^OclIntensity(Chunk|Out|Up|Flying)$/
        ' "$out/$run.log" > "$out/ocl-intensity-$run.txt"
    done
    diff -u "$out/ocl-intensity-continue.txt" "$out/ocl-intensity-resume.txt"
}
