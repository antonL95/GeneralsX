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
            velocityTolerance = 0.00001
            finalSample = last < 420 ? last : 420
            split("Chunk Out Up Flying", cases, " ")
            expectedVelocity["Chunk"] = "0.400000 0.000000 -0.071111"
            expectedVelocity["Out"] = "-2.771648 3.254879 -0.000001"
            expectedVelocity["Up"] = "-0.145121 -0.013962 3.846504"
            expectedVelocity["Flying"] = "-0.290242 -0.027924 2.763296"
            expectedContact["Chunk"] = 258
            expectedContact["Out"] = 244
            expectedContact["Up"] = 353
            expectedContact["Flying"] = 323
        }
        $1 == "TEST" && $2 == "ocl-intensity" && $3 >= launch && $4 ~ /^OclIntensity(Chunk|Out|Up|Flying)$/ {
            frame = $3
            kind = substr($4, length("OclIntensity") + 1)
            if (NF != 11 || $7 !~ /^-?[0-9]+\.[0-9]+$/) {
                fail("invalid " kind " trace at frame " frame ": " $0)
                next
            }
            height = $7 + 0
            if (height < 0)
                fail(kind " below ground at frame " frame ": " $0)
            expectedFrame = kind in previousFrame ? previousFrame[kind] + 1 : launch
            if (frame != expectedFrame)
                fail("expected " kind " sample at frame " expectedFrame ", got " frame)
            if (frame == launch) {
                split(expectedVelocity[kind], velocity)
                for (axis = 1; axis <= 3; ++axis) {
                    component = $(7 + axis)
                    if (component !~ /^-?[0-9]+\.[0-9]+$/ || abs(component - velocity[axis]) > velocityTolerance) {
                        fail("expected " kind " launch velocity " expectedVelocity[kind] " at frame " frame ": " $0)
                        break
                    }
                }
                print "ocl-intensity", kind, "launch velocity", $8, $9, $10
            }
            if (height == 0 && !(kind in landing))
                landing[kind] = frame
            previousFrame[kind] = frame
        }
        END {
            for (i = 1; i <= 4; ++i) {
                kind = cases[i]
                if (previousFrame[kind] != finalSample)
                    fail("missing " kind " sample at frame " finalSample)
                if ((kind in landing || finalSample >= expectedContact[kind]) && landing[kind] != expectedContact[kind])
                    fail("expected " kind " first ground contact at frame " expectedContact[kind] ", got " (kind in landing ? landing[kind] : "none"))
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
