function abs(value) {
    return value < 0 ? -value : value
}

$1 == "TEST" && $2 == "track" && $3 == track_frame && $5 ~ /^GroupStart(Guard|Archers|Knights|Hero)$/ {
    layout = seen[$5]++
    if (layout < 2) {
        anchorLayout[$4] = layout
        anchorName[$4] = $5
    }
}

$1 == "TEST" && $2 == "pos" {
    id = $4
    anchor = $5 == 0 ? id : $5
    if (!(anchor in anchorLayout))
        next
    layout = anchorLayout[anchor]
    frame = $3
    order = layout ? 242 : 240
    elapsed = frame - order
    if (elapsed < 0 || elapsed > 90)
        next
    x = $6
    y = $7
    if (id in prevFrame && prevFrame[id] == frame - 1) {
        dx = x - prevX[id]
        dy = y - prevY[id]
        key = layout SUBSEP elapsed
        if (dx > 0.05 && !(id in firstForward))
            firstForward[id] = elapsed
        if (dx > 0.05 && dx >= abs(dy)) {
            ++aligned[key]
            if (id != anchor || anchorName[anchor] == "GroupStartHero")
                ++physicalAligned[key]
            if (!(id in firstAligned))
                firstAligned[id] = elapsed
        }
        if (dx*dx + dy*dy > 0.05*0.05) {
            if (!(id in firstMoving))
                firstMoving[id] = elapsed
            error = atan2(abs(dy), dx) * 180 / atan2(0, -1)
            ++moving[key]
            headingError[key] += error
            if (error > maxHeadingError[key])
                maxHeadingError[key] = error
        }
        if (!(key in minY) || y < minY[key])
            minY[key] = y
        if (!(key in maxY) || y > maxY[key])
            maxY[key] = y
    }
    prevFrame[id] = frame
    prevX[id] = x
    prevY[id] = y
    if (!(id in actorAnchor)) {
        actorAnchor[id] = anchor
        actorLayout[id] = layout
        ++actors[layout]
    }
}

END {
    for (id in actorAnchor) {
        anchor = actorAnchor[id]
        printf "actor %d %s %d %s moving=%s forward=%s aligned=%s\n", actorLayout[id], anchorName[anchor], id,
            id == anchor ? "anchor" : "member",
            id in firstMoving ? firstMoving[id] : "none",
            id in firstForward ? firstForward[id] : "none",
            id in firstAligned ? firstAligned[id] : "none"
    }
    for (layout = 0; layout < 2; ++layout) {
        printf "cohort %d actors=%d\n", layout, actors[layout]
        for (elapsed = 1; elapsed <= 90; ++elapsed) {
            key = layout SUBSEP elapsed
            if (!(key in minY))
                continue
            printf "frame %d %d moving=%d aligned=%d meanError=%.2f maxError=%.2f lateralWidth=%.2f physicalAligned=%d\n",
                layout, elapsed, moving[key], aligned[key], moving[key] ? headingError[key] / moving[key] : 0,
                maxHeadingError[key], maxY[key] - minY[key], physicalAligned[key]
        }
    }
}
