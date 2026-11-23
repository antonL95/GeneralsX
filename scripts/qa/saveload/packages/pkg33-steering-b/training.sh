prepare() {
    awk '
        BEGIN {
            for (frame = 35; frame <= 59; ++frame)
                sample(frame, "GondorKnightHorde")
            for (frame = 127; frame <= 180; ++frame)
                sample(frame, "GondorKnightHorde")
            for (frame = 238; frame <= 249; ++frame)
                sample(frame, "GondorPippin")
            for (frame = 293; frame <= 303; ++frame)
                sample(frame, "GondorPippin")
            for (frame = 342; frame <= 347; ++frame)
                sample(frame, "GondorPippin")
            for (frame = 361; frame <= 372; ++frame)
                sample(frame, "GondorPippin")
            for (frame = 448; frame <= 462; ++frame)
                sample(frame, "MordorAttackTroll")
        }
        function sample(frame, actor) {
            print frame, "steering-path Player_1", actor, "0"
        }
    '
}

check() {
    covers 462 || return 0
    awk -v first="$first" '
        $1 == "TEST" && $2 == "steering" && $4 == "GondorKnightHorde" {
            speed = $9
            sub("velocity=", "", speed)
            speed += 0
            threshold = 100 / 30 * 0.65
            if ($3 >= 35 && $3 <= 59 && previousSpeed < threshold && speed > previousSpeed && /accel=1 decel=0/)
                cavAccelerated = 1
            if ($3 >= 127 && $3 <= 180 && previousSpeed >= threshold && speed < threshold && /accel=0 decel=1/)
                cavBrakedAtThreshold = 1
            previousSpeed = speed
        }
        $1 == "TEST" && $2 == "steering" && $4 == "GondorPippin" {
            if ($3 == 238 && /left=0 right=0 leftHS=0 rightHS=0/)
                aligned = 1
            if ($3 == 244 && /left=0 right=0 leftHS=1 rightHS=0/)
                leftFast = 1
            if ($3 >= 293 && $3 <= 303 && /left=1 right=0 leftHS=0 rightHS=0/)
                leftSlow = 1
            if ($3 >= 342 && $3 <= 344 && /left=0 right=0 leftHS=0 rightHS=1/)
                rightFast = 1
            if ($3 == 346 && /left=0 right=0 leftHS=0 rightHS=0/)
                stopped = 1
            if ($3 >= 361 && $3 <= 372 && /left=0 right=1 leftHS=0 rightHS=0/)
                rightSlow = 1
        }
        $1 == "TEST" && $2 == "steering" && $4 == "MordorAttackTroll" {
            if ($3 == 448) {
                speed = $9
                sub("velocity=", "", speed)
                trollReady = speed > 65 / 60 && $10 ~ /^loco=TrollLocomotor:/
                initialHeading = $6
            }
            if ($3 >= 451 && $3 <= 455 && $6 != initialHeading)
                trollTurned = 1
            if (/leftHS=1|rightHS=1/)
                trollFast = 1
        }
        END {
            gait = first > 35 || (cavAccelerated && cavBrakedAtThreshold)
            exit !(gait && (first > 238 || aligned) && leftFast && leftSlow && rightFast && stopped && rightSlow && trollReady && trollTurned && !trollFast)
        }
    ' "$log"
}

compare() {
    for run in continue resume; do
        awk -v frame="$save_frame" '$1 == "TEST" && $2 == "steering" && $3 >= frame && ($4 == "GondorPippin" || $4 == "MordorAttackTroll")' "$out/$run.log" > "$out/steering-b-$run.txt"
    done
    cmp "$out/steering-b-continue.txt" "$out/steering-b-resume.txt"
}
