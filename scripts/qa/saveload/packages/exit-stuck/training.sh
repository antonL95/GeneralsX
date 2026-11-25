prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 960; frame <= end && frame <= 1440; frame += 20) {
            print frame, "exit-stuck-sample Player_1 ShireShirriffsQuarters 0 " (frame > 1250 ? "1700,3330" : "-")
            print frame, "exit-stuck-sample Player_1 RivendellDunedainGathering 0 " "-"
            print frame, "exit-stuck-sample Player_1 GondorStable 0 " (frame > 1250 ? "2500,3150" : "-")
        }
        if (end >= 1445) {
            print 1445, "destroy Player_1 ShireBounderHorde 0"
            print 1445, "destroy Player_1 ShireBounderHorde 1"
            print 1445, "destroy Player_1 ShireBounderHorde 2"
            print 1445, "destroy Player_1 ArnorRangerHorde 0"
            print 1445, "destroy Player_1 ArnorRangerHorde 1"
            print 1445, "destroy Player_1 ArnorRangerHorde 2"
            print 1445, "destroy Player_1 GondorPinnathGelinRiderHorde 0"
            print 1445, "destroy Player_1 GondorPinnathGelinRiderHorde 1"
            print 1445, "destroy Player_1 GondorPinnathGelinRiderHorde 2"
            for (i = 0; i < 12; ++i) print 1445, "destroy Player_1 GondorFighter 0"
            print 1446, "destroy Player_1 ShireShirriffsQuarters 0"
            print 1446, "destroy Player_1 RivendellDunedainGathering 0"
            print 1446, "destroy Player_1 GondorStable 0"
        }
    }'
}

check() {
    covers 1445 || return 0
    awk '
        $1 == "TEST" && $2 == "exit-stuck" {
            frame = $3 + 0
            id = $5
            if ($4 == "RivendellDunedainGathering") next
            split($7, slotdist, "="); split($8, producerdist, "="); split($9, footprint, "="); split($10, destdist, "=")
            if (frame == 1440) {
                seen[id] = 1
                if (slotdist[2] > 20 && producerdist[2] < footprint[2] + 5) { printf "%s still inside the building at 1440: %s\n", id, $0; failed = 1 }
                if (slotdist[2] > 80) { printf "%s far from its slot at 1440: %s\n", id, $0; failed = 1 }
            }
            if (frame == 1440) {
                after[id] = 1
                if (destdist[2] > 150) { printf "%s did not follow the move order: %s\n", id, $0; failed = 1 }
            }
        }
        END {
            for (id in seen) total++
            if (total < 20) { printf "Only %d members sampled at 1440\n", total; failed = 1 }
            exit failed
        }
    ' "$log"
}
