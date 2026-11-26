prepare() {
    [ "$end" -ge 3000 ] || return 0
    awk -v end="$end" 'BEGIN {
        for (frame = 300; frame <= end && frame <= 3000; frame += 20) {
            print frame, "exit-stuck-sample Player_1 MirkwoodRoyalBarracks 0 -"
            print frame, "exit-stuck-sample Player_1 MirkwoodTrainingGrounds 0 -"
        }
    }'
}

check() {
    covers 3000 || return 0
    awk '
        function field(name,    i, parts) {
            for (i = 6; i <= NF; ++i) {
                split($i, parts, "=")
                if (parts[1] == name) return parts[2]
            }
            return ""
        }
        $1 == "TEST" && $2 == "exit-stuck" {
            frame = $3 + 0
            id = $5
            slot[frame, id] = field("slotdist")
            moving[frame, id] = field("moving")
            split(field("pos"), xy, ",")
            px[frame, id] = xy[1]; py[frame, id] = xy[2]
            ids[id] = 1
            ++samples
            if (moving[frame, id] == 1 && moving[frame - 20, id] == 1 && moving[frame - 40, id] == 1) {
                dx = px[frame, id] - px[frame - 40, id]; dy = py[frame, id] - py[frame - 40, id]
                if (dx * dx + dy * dy < 9) { printf "%d %s runs in place\n", frame, id; failed = 1 }
            }
        }
        END {
            if (!samples) { print "no exit-stuck samples"; failed = 1 }
            n = split("1440 2180 3000", marks, " ")
            for (m = 1; m <= n; ++m)
                for (id in ids) {
                    f = marks[m]
                    if (!((f, id) in slot)) continue
                    if (slot[f, id] > 60) { printf "%s far from its slot at %d (%s)\n", id, f, slot[f, id]; failed = 1 }
                }
            exit failed
        }
    ' "$log"
}
