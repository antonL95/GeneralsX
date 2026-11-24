prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 91; frame <= end && frame <= 1907; ++frame) {
            print frame, "ambient-roam-sample Player_1 AmbientRoamHall 0"
            if (frame >= 101)
                print frame, "ambient-roam-sample Player_1 ElvenGreenPasture 0"
        }
    }'
}

check() {
    awk -v first="$first" -v end="$end" -v checkpoint="$save_frame" '
        $1 == "TEST" && $2 == "ambient-roam" {
            frame = $3
            id = $5
            if (!(id in species)) {
                species[id] = $4
                starts[id] = frame
                count[$4]++
            }
            if (id in last && frame != last[id] + 1) {
                print "Ambient actor", id, "missing a sample before frame", frame
                failed = 1
            }
            last[id] = frame
            if ($8 > peak[id]) {
                peak[id] = $8
                peak_frame[id] = frame
            }
            if ($4 == "RivendellTownsman" && $10 && !$13) {
                roaming[id] = 1
                goal = sqrt($11 * $11 + $12 * $12)
                if (frame == checkpoint && goal <= 100.05)
                    saved_roaming = 1
                if (goal > goal_peak[id]) {
                    goal_peak[id] = goal
                    goal_frame[id] = frame
                }
            }
            if ($4 == "HorseGrazing_SlavedforPasture" && ($9 || $10)) {
                print "Pasture horse", id, "has a movement order at frame", frame
                failed = 1
            }
        }
        END {
            if (count["RivendellTownsman"] != 8 || count["HorseGrazing_SlavedforPasture"] != 3) {
                print "Expected eight townsmen and three horses, got", count["RivendellTownsman"] + 0,
                    "and", count["HorseGrazing_SlavedforPasture"] + 0
                failed = 1
            }
            if (!saved_roaming) {
                print "No townsman was wandering at checkpoint", checkpoint
                failed = 1
            }
            finish = end < 1907 ? end : 1907
            for (id in species) {
                limit = species[id] == "RivendellTownsman" ? 140 : 50
                if (last[id] != finish || (first > 0 && starts[id] != first)) {
                    print species[id], id, "incomplete sampling", starts[id], last[id]
                    failed = 1
                }
                if (peak[id] > limit) {
                    print species[id], id, "peak", peak[id], "exceeds", limit, "at frame", peak_frame[id]
                    failed = 1
                }
                if (species[id] == "RivendellTownsman" && (!roaming[id] || goal_peak[id] > 100.05)) {
                    print species[id], id, "expected a guard goal within 100 units, peak",
                        goal_peak[id] + 0, "at frame", goal_frame[id] + 0
                    failed = 1
                }
            }
            exit failed
        }
    ' "$log"
}
