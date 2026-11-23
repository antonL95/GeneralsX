check() {
    awk '
        function inside(x, y, cx, cy, hx, hy) {
            return x > cx - hx && x < cx + hx && y > cy - hy && y < cy + hy
        }
        $1 == "TEST" && $2 == "track" && $5 ~ /^BuildingWalk(Single|Group)$/ {
            actor[$4] = $5
        }
        $1 == "TEST" && $2 == "pos" && $4 in actor {
            frame = $3
            x = $6
            y = $7
            if (inside(x, y, 2446.27, 1415.8, 38.7, 58) ||
                inside(x, y, 2416.7, 1467.6, 12.25, 12.25)) {
                printf "%s %s entered the barracks at frame %s (%s, %s)\n", actor[$4], $4, frame, x, y
                failed = 1
                exit
            }
            if (x > 2570 && $10 == 0)
                arrived[$4] = 1
        }
        END {
            if (failed)
                exit 1
            for (id in actor)
                if (arrived[id])
                    count[actor[id]]++
            if (count["BuildingWalkSingle"] != 1 || count["BuildingWalkGroup"] != 2) {
                printf "Settled past the barracks: %d single movers and %d group members\n",
                    count["BuildingWalkSingle"], count["BuildingWalkGroup"]
                exit 1
            }
        }
    ' "$log" &&
    expect "TEST save-load obstacle frame=245 template=BuildingWalkBarracks ownsCell=1"
}
