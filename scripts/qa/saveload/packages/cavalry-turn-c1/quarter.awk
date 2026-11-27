function abs(value) { return value < 0 ? -value : value }
function max(a, b) { return a > b ? a : b }
function min(a, b) { return a < b ? a : b }
function distance(dx, dy) { return sqrt(dx*dx + dy*dy) }
function headingError(facing, goal, delta) {
    delta = abs(facing - goal)
    return delta > pi ? 2*pi - delta : delta
}

BEGIN {
    first = 620
    last = 920
    pi = atan2(0, -1)
}

$1 == "TEST" && $2 == "cavalry-c1-quarter" {
    frame = $3
    id = $5
    x = $7
    y = $8
    if ($4 == "anchor") {
        anchorX[frame] = x
        anchorY[frame] = y
        anchorYaw[frame] = $9
        anchorMoving[frame] = $15
    } else if ($4 == "banner") {
        bannerX[frame] = x
        bannerY[frame] = y
    } else if ($4 == "rider" && $6 >= 0) {
        if (frame == first) ids[++riders] = id
        riderX[frame, id] = x
        riderY[frame, id] = y
        riderYaw[frame, id] = $9
        riderTurnStep[id] = $12
        riderRank[frame, id] = ($6 < 5) ? 1 : 2
        ++samples[frame]
        slotError[frame] = max(slotError[frame], distance($10 - x, $11 - y))
    }
}

END {
    if (riders != 10 || abs(anchorYaw[first]) > 0.05 || anchorMoving[first]) {
        print "Quarter turn expected ten settled riders facing east"
        exit 1
    }
    for (frame = first; frame <= last; ++frame) {
        if (!(frame in anchorX) || !(frame in bannerX) || samples[frame] != riders) {
            print "Incomplete bannered cavalry trace at frame " frame
            exit 1
        }
        if (aligned) continue
        turning = 0
        for (i = 1; i <= riders; ++i) {
            id = ids[i]
            x = riderX[frame, id]
            y = riderY[frame, id]
            if (headingError(riderYaw[frame, id], anchorYaw[last]) > riderTurnStep[id]) ++turning
            riderTravel = max(riderTravel, distance(x - riderX[first, id], y - riderY[first, id]))
            for (j = i + 1; j <= riders; ++j) {
                other = ids[j]
                gap = distance(x - riderX[frame, other], y - riderY[frame, other])
                if (!minGapSeen || gap < minGap) minGap = gap
                minGapSeen = 1
            }
        }
        if (frame == first) initialGap = minGap
        if (!turning) aligned = frame
    }

    turnEnd = aligned ? aligned : last
    printf "Pinnath Gelin quarter turn: aligned=%d anchor travel=%.3f max rider displacement=%.3f min rider spacing=%.3f initially=%.3f banner travel=%.3f\n",
        aligned, distance(anchorX[turnEnd] - anchorX[first], anchorY[turnEnd] - anchorY[first]), riderTravel, minGap, initialGap,
        distance(bannerX[turnEnd] - bannerX[first], bannerY[turnEnd] - bannerY[first])

    forwardX = cos(anchorYaw[last])
    forwardY = sin(anchorYaw[last])
    for (i = 1; i <= riders; ++i) {
        id = ids[i]
        rank = riderRank[last, id]
        x = riderX[last, id]
        y = riderY[last, id]
        forward = x*forwardX + y*forwardY
        if (++rankSize[rank] == 1) {
            rankLow[rank] = rankHigh[rank] = forward
        } else {
            rankLow[rank] = min(rankLow[rank], forward)
            rankHigh[rank] = max(rankHigh[rank], forward)
        }
        if (rank == 1) frontSide += y*forwardX - x*forwardY
    }
    if (rankSize[1] > 0) frontSide /= rankSize[1]
    bannerSide = bannerY[last]*forwardX - bannerX[last]*forwardY
    bannerForward = bannerX[last]*forwardX + bannerY[last]*forwardY
    printf "Pinnath Gelin finish: ranks=%d+%d yaw=%.4f moving=%d row span=%.3f/%.3f slot error=%.3f banner center offset=%.3f ahead=%.3f\n",
        rankSize[1], rankSize[2], anchorYaw[last], anchorMoving[last], rankHigh[1] - rankLow[1], rankHigh[2] - rankLow[2],
        slotError[last], bannerSide - frontSide, bannerForward - rankHigh[1]
}
