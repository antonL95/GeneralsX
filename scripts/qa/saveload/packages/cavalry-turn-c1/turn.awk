function abs(value) { return value < 0 ? -value : value }
function max(a, b) { return a > b ? a : b }
function min(a, b) { return a < b ? a : b }
function distance(x, y) { return sqrt(x*x + y*y) }
function angular_error(a, b, delta) {
    delta = abs(a - b)
    return delta > pi ? 2*pi - delta : delta
}
function require(condition, message) {
    if (!condition) {
        print message
        failed = 1
    }
}

BEGIN {
    pi = atan2(0, -1)
    first = 238
    last = 419
    settle = 2
    rounding = 0.001
    pathCell = 10
    framesPerRetailStep = 30/5
}

$1 == "TEST" && $2 == "cavalry-c1" {
    frame = $3
    role = $4
    id = $5
    x = $7
    y = $8
    facing = $9
    if (role == "anchor") {
        anchorX[frame] = x
        anchorY[frame] = y
        anchorYaw[frame] = facing
        creep = $13 * 0.1
    } else if (role == "hero") {
        heroX[frame] = x
        heroY[frame] = y
        heroYaw[frame] = facing
        heroStep = $12
        if ($14) ++mountedFrames
        heroMoving[frame] = $15
    } else if ($6 >= 0) {
        if (frame == first) {
            ids[++riders] = id
            startsX[id] = x
            startsY[id] = y
            centerX += x
            initialError = max(initialError, distance($10 - x, $11 - y))
        }
        riderX[frame, id] = x
        riderY[frame, id] = y
        riderYaw[frame, id] = facing
        slotError[frame] = max(slotError[frame], distance($10 - x, $11 - y))
        yawError[frame] = max(yawError[frame], angular_error(facing, pi))
        ++riderSamples[frame]
        riderStep = $12
    } else {
        ++extraRiders
    }
}

END {
    require(riders == 10 && !extraRiders && initialError <= settle + rounding,
        sprintf("Expected ten settled Swan Knights, found %d with %.3f maximum initial slot error", riders, initialError))
    require(mountedFrames == last - 235 + 1 && heroStep > 0 && riderStep > 0 && creep > 0,
        "Missing mounted hero or authored locomotor data")
    if (failed) exit 1

    centerX /= riders
    for (i = 1; i <= riders; ++i) {
        id = ids[i]
        row[id] = startsX[id] > centerX ? 1 : 2
        ++rowSize[row[id]]
    }
    require(rowSize[1] == 5 && rowSize[2] == 5, "Initial cavalry ranks must be two rows of five")
    for (frame = first; frame <= last; ++frame) {
        require(frame in anchorX && frame in heroX && riderSamples[frame] == riders,
            "Incomplete cavalry trace at frame " frame)
        if (!snap && angular_error(anchorYaw[frame], pi) <= riderStep)
            snap = frame
        if (!aligned && yawError[frame] <= framesPerRetailStep*riderStep)
            aligned = frame
        if (!turned && yawError[frame] <= riderStep)
            turned = frame

        delete west
        delete east
        for (i = 1; i <= riders; ++i) {
            id = ids[i]
            x = riderX[frame, id]
            y = riderY[frame, id]
            rank = row[id]
            if (rank in west) {
                west[rank] = min(west[rank], x)
                east[rank] = max(east[rank], x)
            } else {
                west[rank] = east[rank] = x
            }
            if (frame > first && !(id in advances) && x < riderX[frame - 1, id] - rounding) {
                advances[id] = frame
                if (!firstAdvance) firstAdvance = frame
                lastAdvance = frame
                ++advancingRiders
            }
            if (!aligned) {
                riderTravel = max(riderTravel, distance(x - startsX[id], y - startsY[id]))
                riderDrift = max(riderDrift, distance(x - startsX[id] - anchorX[frame] + anchorX[first],
                    y - startsY[id] - anchorY[frame] + anchorY[first]))
            }
            if (!turned) {
                columnDrift = max(columnDrift, abs(y - startsY[id]))
                if (frame > first)
                    riderYawJump = max(riderYawJump, angular_error(riderYaw[frame, id], riderYaw[frame - 1, id]))
            }
        }
        if (west[1] <= east[2] && !crossed)
            crossed = frame
        rowBow = max(rowBow, max(east[1] - west[1], east[2] - west[2]))
    }
    creepTravel = distance(anchorX[aligned] - anchorX[first], anchorY[aligned] - anchorY[first])
    printf "Swan Knights snap=%d aligned=%d turned=%d anchor=%.3f rider=%.3f rider drift=%.3f bow=%.3f column drift=%.3f crossed=%d advance=%d..%d final error=%.3f\n",
        snap, aligned, turned, creepTravel, riderTravel, riderDrift, rowBow, columnDrift, crossed, firstAdvance, lastAdvance, slotError[last]
    require(snap && aligned > snap && turned >= aligned, "Cavalry did not snap then align its riders")
    require(creepTravel <= creep * (aligned - first + 1) + rounding, "Cavalry anchor exceeded startup creep during the turn")
    require(riderTravel <= creepTravel + settle + rounding && rowBow <= settle + rounding &&
        columnDrift <= settle + rounding && riderYawJump <= riderStep + rounding && !crossed,
        "Swan Knights did not preserve their world-space ranks through the reversal")
    require(advancingRiders == riders && firstAdvance >= turned && lastAdvance - firstAdvance <= 1,
        "Swan Knights did not step forward together after turning")
    require(distance(anchorX[last] - (anchorX[first] - 120), anchorY[last] - anchorY[first]) <= pathCell &&
        slotError[last] <= settle + rounding,
        "Swan Knights did not arrive in their two by five layout")

    previousError = angular_error(heroYaw[239], 0)
    require(abs(previousError - pi/3) <= heroStep, "Initial hero heading was not sixty degrees from the order")
    for (frame = 240; frame <= last; ++frame) {
        dx = heroX[frame] - heroX[frame - 1]
        dy = heroY[frame] - heroY[frame - 1]
        previousYaw = heroYaw[frame - 1]
        reverse = max(reverse, -(dx*cos(previousYaw) + dy*sin(previousYaw)))
        error = angular_error(heroYaw[frame], 0)
        if (!heroAligned) {
            yawJitter = max(yawJitter, error - previousError)
            yawJump = max(yawJump, angular_error(heroYaw[frame], heroYaw[frame - 1]))
            arc = max(arc, abs(heroY[frame] - heroY[239]))
            if (dx > rounding || abs(dy) > rounding) ++movingWhileTurning
            if (error <= heroStep) heroAligned = frame
        }
        previousError = error
    }
    printf "Imrahil aligned=%d reverse=%.5f yaw jitter=%.5f yaw jump=%.5f arc=%.3f turning travel frames=%d final=(%.3f,%.3f)\n",
        heroAligned, reverse, yawJitter, yawJump, arc, movingWhileTurning, heroX[last], heroY[last]
    require(heroAligned && movingWhileTurning > 1 && arc > rounding && reverse <= rounding &&
        yawJitter <= rounding && yawJump <= heroStep + rounding,
        "Imrahil did not turn smoothly into travel without reversing")
    require(distance(heroX[last] - 2750, heroY[last] - 1000) <= pathCell && !heroMoving[last],
        "Imrahil did not reach the ordered position")
    exit failed
}
