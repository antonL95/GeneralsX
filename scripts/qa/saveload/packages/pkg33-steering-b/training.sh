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
            for (frame = 504; frame <= 518; ++frame)
                print frame, "steering-reaction Player_1 BlackrootValeArcherHorde 0"
        }
        function sample(frame, actor) {
            print frame, "steering-path Player_1", actor, "0"
        }
    '
}

check() {
    covers 462 || return 0
    check_archer=0
    if covers 518; then
        check_archer=1
    fi
    awk -v first="$first" -v checkArcher="$check_archer" '
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
            if ($3 >= 293 && $3 <= 303 && /left=1 right=0 leftHS=0 rightHS=0/)
                leftSlow = 1
            if ($3 >= 342 && $3 <= 344 && /left=0 right=0 leftHS=0 rightHS=1/)
                rightFast = 1
            if ($3 == 346 && /left=0 right=0 leftHS=0 rightHS=0/)
                stopped = 1
            if ($3 >= 361 && $3 <= 372 && /left=0 right=1 leftHS=0 rightHS=0/)
                rightSlow = 1
            if ($3 >= 238 && $3 <= 249) {
                split(substr($5, 5), position, ",")
                split(substr($7, 6), goal, ",")
                facing = substr($6, 8)
                pi = atan2(0, -1)
                if (headingFrame == $3 - 1 && /leftHS=1/) {
                    headingStep = facing - previousFacing
                    headingStep = atan2(sin(headingStep), cos(headingStep))
                    headingLimit = 2 * pi / (3 * 6) + 0.0001
                    if (headingStep < -headingLimit || headingStep > headingLimit)
                        yawJump = 1
                    if (headingStep > 0.0001)
                        yawSteps++
                }
                previousFacing = facing
                headingFrame = $3
                if ($3 == 244 && /left=0 right=0 leftHS=1 rightHS=0/) {
                    aimX = goal[1] - position[1]
                    aimY = goal[2] - position[2]
                    forward = aimX * cos(facing) + aimY * sin(facing)
                    lateral = aimY * cos(facing) - aimX * sin(facing)
                    leftFast = atan2(lateral, forward) > atan2(0, -1) / 12
                }
                dx = position[1] - previousX
                dy = position[2] - previousY
                toX = goal[1] - previousX
                toY = goal[2] - previousY
                if ($3 > 240 && $3 <= 244 && /leftHS=1/ && dx * dx + dy * dy > 0) {
                    along = dx * toX + dy * toY
                    forward = dx * cos(facing) + dy * sin(facing)
                    turnedTowardGoal = dy * toY > 0
                    if (along > 0 && forward > 0 && turnedTowardGoal)
                        translatedTowardGoal = 1
                }
                previousX = position[1]
                previousY = position[2]
            }
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
        $1 == "TEST" && $2 == "steering-reaction" && $4 == "BlackrootValeArcherHorde" {
            if ($3 == 504 && /velocity=0.00000,0.00000/)
                archerStationary = 1
            if ($3 >= 505 && $3 <= 518 && /path=1 waiting=0 groupRoute=0/ && /reform=0/ &&
                $7 != "velocity=0.00000,0.00000")
                archerDisplaced = 1
        }
        END {
            gait = first > 35 || (cavAccelerated && cavBrakedAtThreshold)
            if (first <= 238 && !translatedTowardGoal)
                print "Pippin did not translate toward the route during the high speed turn at the checkpoint"
            if (!leftFast)
                print "Pippin did not have a remaining high speed turn at the frame 244 checkpoint"
            if (first <= 238 && (yawJump || yawSteps < 2))
                print "Pippin's high speed turn did not advance in bounded render frame steps"
            if (checkArcher && first <= 504 && !(archerStationary && archerDisplaced))
                print "Idle archer horde did not start its independent sharp move by frame 518"
            exit !(gait && (first > 238 || (aligned && translatedTowardGoal && !yawJump && yawSteps >= 2)) &&
                leftFast && leftSlow && rightFast && stopped && rightSlow && trollReady && trollTurned && !trollFast &&
                (!checkArcher || first > 504 || (archerStationary && archerDisplaced)))
        }
    ' "$log"
}

compare() {
    for run in continue resume; do
        awk -v frame="$save_frame" '$1 == "TEST" && $2 == "steering" && $3 >= frame && ($4 == "GondorPippin" || $4 == "MordorAttackTroll")' "$out/$run.log" > "$out/steering-b-$run.txt"
    done
    cmp "$out/steering-b-continue.txt" "$out/steering-b-resume.txt"
}
