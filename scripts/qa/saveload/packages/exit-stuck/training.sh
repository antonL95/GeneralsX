prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 1200; frame <= end && frame <= 2780; frame += 20) {
            print frame, "exit-stuck-sample Player_1 ElvenBarracks 0 -"
            print frame, "exit-stuck-sample Player_1 ElvenBarracks 1 -"
            print frame, "exit-stuck-sample Player_1 ElvenBarracks 2 -"
        }
    }'
}

check() {
    covers 2796 || return 0
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
            dest[frame, id] = field("destdist")
            prod[frame, id] = field("producerdist")
            foot = field("footprint")
            moving[frame, id] = field("moving")
            split(field("pos"), xy, ",")
            px[frame, id] = xy[1]; py[frame, id] = xy[2]
            ids[id] = 1
            if (frame >= 1240 && moving[frame, id] == 1 && moving[frame - 20, id] == 1 && moving[frame - 40, id] == 1) {
                dx = px[frame, id] - px[frame - 40, id]; dy = py[frame, id] - py[frame - 40, id]
                if (dx * dx + dy * dy < 9) { printf "%d %s runs in place\n", frame, id; failed = 1 }
            }
        }
        END {
            n = split("1800 2780", marks, " ")
            for (m = 1; m <= n; ++m)
                for (id in ids) {
                    f = marks[m]
                    if (!((f, id) in slot)) continue
                    if (slot[f, id] > 60) { printf "%s far from its slot at %d (%s)\n", id, f, slot[f, id]; failed = 1 }
                }
            for (id in ids) {
                if (((2780, id) in dest) && dest[2780, id] > 260) { printf "%s did not follow the last order (%s)\n", id, dest[2780, id]; failed = 1 }
                if (((1800, id) in dest) && dest[1800, id] > 260) { printf "%s did not follow the first order (%s)\n", id, dest[1800, id]; failed = 1 }
            }
            exit failed
        }
    ' "$log"
}
