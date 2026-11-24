prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 212; frame <= end; ++frame) {
            print frame, "production-exit-sample Player_1 ShireShirriffsQuarters 0"
            print frame, "production-exit-sample Player_1 RivendellDunedainGathering 0"
            print frame, "production-exit-sample Player_1 RivendellDormitory 0"
            print frame, "production-exit-sample Player_1 RivendellBreeLandEncampment 0"
            print frame, "production-exit-sample Player_1 GondorStable 0"
            if (frame >= 1268 && frame < 1300)
                print frame, "production-exit-kill-member Player_1 ArnorRangerHorde 1"
        }
    }'
}

check() {
    if [ "$phase" != continue ]; then return 0; fi
    awk '
        $1 == "TEST" && $2 == "production-exit" {
            producer = $4; id = $5; dx = $6 - $8; dy = $7 - $9
            if (!(id in seen)) {
                seen[id] = 1; count[producer]++
                owner[id] = producer
                if (++released[producer, $3] > 1) {
                    if (producer == "GondorStable") burst = 1
                    else { print "Multiple members first observed in one frame:", producer, $3; failed = 1 }
                }
                startX[id] = $6; startY[id] = $7
                directionX[id] = $10 - $8; directionY[id] = $11 - $9
                if (dx * dx + dy * dy > 100) {
                    print "Member first observed away from exit:", $0
                    failed = 1
                }
            }
            dot = ($6 - startX[id]) * directionX[id] + ($7 - startY[id]) * directionY[id]
            if (dot >= 10 * sqrt(directionX[id] * directionX[id] + directionY[id] * directionY[id]))
                moved[id] = 1
            if (producer == "ShireShirriffsQuarters" && $3 >= 2000 && dx * dx + dy * dy > 200 * 200)
                escaped[id] = 1
        }
        $1 == "TEST" && $2 == "production-cavalry" {
            if ($3 == 250) { cavalryX = $4; cavalryY = $5; cavalryBefore = 1 }
            if ($3 == 500) {
                dx = $4 - cavalryX; dy = $5 - cavalryY
                cavalryAfter = cavalryBefore && dx * dx + dy * dy > 100 * 100 && $6 > 104.9 && $6 < 105.1
            }
        }
        $1 == "TEST" && $2 == "production-exit-retired" {
            if ($3 == 1310) { retiredX = $4; retiredY = $5; retiredBefore = ($6 == "selectable=1" && $7 == "collisions=1" && $8 != "members=0") }
            if ($3 == 1600) {
                dx = $4 - retiredX; dy = $5 - retiredY
                retiredAfter = retiredBefore && $6 == "selectable=1" && $7 == "collisions=1" && dx * dx + dy * dy > 100 * 100
            }
        }
        $1 == "TEST" && $2 == "production-exit-queue-state" {
            if ($3 == 1285) queuePending = ($4 == "active=1")
            if ($3 == 1450) queueComplete = ($4 == "active=0")
        }
        $1 == "TEST" && $2 == "production-exit-anchor" {
            if ($3 == 1273) emptyAnchorStarted = ($4 == "exists=1")
            if ($3 == 1284) emptyAnchorRemoved = ($4 == "exists=0")
        }
        $1 == "TEST" && $2 == "production-exit-casualty" {
            if ($4 == "removed=1" && $5 == "members=0") { casualtyRemoved = 1; casualtyFrame = $3 }
            if (casualtyRemoved && $3 > casualtyFrame && $5 == "members=0") survivedLoss = 1
            if (survivedLoss && $5 != "members=0") continuedRelease = 1
        }
        END {
            expected["ShireShirriffsQuarters"] = 12
            expected["RivendellDunedainGathering"] = 12
            expected["RivendellDormitory"] = 10
            expected["RivendellBreeLandEncampment"] = 12
            expected["GondorStable"] = 10
            for (name in expected)
                if (count[name] != expected[name]) { print "Wrong produced member count:", name, count[name], "expected", expected[name]; failed = 1 }
            for (id in seen) {
                if (!moved[id]) { print "Member did not move toward the natural rally:", id; failed = 1 }
                if (owner[id] == "ShireShirriffsQuarters" && !escaped[id]) { print "Bounder did not path away:", id; failed = 1 }
            }
            if (!cavalryAfter) { print "Cavalry did not move at its authored maximum speed"; failed = 1 }
            if (!retiredAfter) { print "Horde from destroyed producer remained locked or could not move"; failed = 1 }
            if (!casualtyRemoved || !survivedLoss || !continuedRelease) { print "Horde failed to continue after losing its first member"; failed = 1 }
            if (!queuePending || !queueComplete) { print "Production queue did not remain occupied through release"; failed = 1 }
            if (!emptyAnchorStarted || !emptyAnchorRemoved) { print "Empty horde survived producer destruction before first member"; failed = 1 }
            if (!burst) { print "Stable did not honor its authored initial burst"; failed = 1 }
            if (failed) exit 1
            print "production-exit: members start at exits and Bounders path away"
        }
    ' "$log"
}
