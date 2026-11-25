prepare() {
    awk -v end="$end" 'BEGIN {
        long = end >= 2400
        last = long ? 2380 : 1440
        destroy = long ? 2395 : 1445
        for (frame = 960; frame <= end && frame <= last; frame += 20) {
            print frame, "exit-stuck-sample Player_1 ShireShirriffsQuarters 0 " (frame > 1250 ? "1700,3330" : "-")
            print frame, "exit-stuck-sample Player_1 RivendellDunedainGathering 0 " "-"
            print frame, "exit-stuck-sample Player_1 GondorStable 0 " (frame > 1250 ? "2500,3150" : "-")
        }
        if (end >= destroy) {
            for (i = 0; i < 3; ++i) {
                print destroy, "destroy Player_1 ShireBounderHorde 0"
                print destroy, "destroy Player_1 ArnorRangerHorde 0"
                print destroy, "destroy Player_1 GondorPinnathGelinRiderHorde 0"
            }
            for (i = 0; i < 12; ++i) print destroy, "destroy Player_1 GondorFighter 0"
            print destroy + 1, "destroy Player_1 ShireShirriffsQuarters 0"
            print destroy + 1, "destroy Player_1 RivendellDunedainGathering 0"
            print destroy + 1, "destroy Player_1 GondorStable 0"
        }
    }'
}

check() {
    covers 1445 || return 0
    final=1440
    [ "$end" -ge 2400 ] && final=2380
    awk -v final="$final" '
        $1 == "TEST" && $2 == "exit-stuck" {
            frame = $3 + 0
            id = $5
            if ($4 == "RivendellDunedainGathering") next
            split($7, slotdist, "="); split($8, producerdist, "="); split($9, footprint, "="); split($10, destdist, "=")
            if (frame == final) {
                seen[id] = 1
                if (slotdist[2] > 20 && producerdist[2] < footprint[2] + 5) { printf "%s still inside the building at %d: %s\n", id, final, $0; failed = 1 }
                if (slotdist[2] > 80) { printf "%s far from its slot at %d: %s\n", id, final, $0; failed = 1 }
                if (destdist[2] > 220) { printf "%s did not follow the move order: %s\n", id, $0; failed = 1 }
            }
        }
        END {
            for (id in seen) total++
            if (total < (final > 2000 ? 40 : 20)) { printf "Only %d members sampled at %d\n", total, final; failed = 1 }
            exit failed
        }
    ' "$log"
}
