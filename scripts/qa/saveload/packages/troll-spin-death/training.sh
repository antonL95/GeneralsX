prepare() {
    awk 'BEGIN {
        for (frame = 207; frame <= 345; ++frame)
            print frame, "troll-spin-state PlyrCreeps GoblinCaveTroll 0"
        for (frame = 944; frame <= 945; ++frame)
            print frame, "troll-spin-state PlyrCreeps GoblinCaveTroll 0"
        for (frame = 1394; frame <= 1395; ++frame)
            print frame, "troll-spin-state PlyrCreeps GoblinCaveTroll 0"
    }'
}

check() {
    for sample in '206 GoblinCaveTroll 2425.0' '220 GoblinCaveTroll 0.0' '250 GoblinCaveTroll 0.0' \
        '357 TrollSpinFixtureVulnerable 2425.0' '358 TrollSpinFixtureVulnerable 75.0' \
        '360 TrollSpinFixtureVulnerable 2500.0'; do
        if covers "${sample%% *}"; then
            expect "TEST troll-spin hit $sample" || return 1
        fi
    done

    awk -v first="$first" -v end="$end" '
        BEGIN {
            pulse[231] = pulse[261] = pulse[285] = pulse[315] = 1
            unitDamage[261] = unitDamage[285] = 200
            buildingDamage[285] = buildingDamage[315] = 200
            unit = building = 2000
            for (frame = 207; frame <= 344; ++frame) {
                if (pulse[frame])
                    shot = frame - 1
                unit -= unitDamage[frame]
                building -= buildingDamage[frame]
                expectedShot[frame] = shot + 0
                expectedUnit[frame] = unit
                expectedBuilding[frame] = building
            }
        }
        function fail(message) {
            print "Troll spin " message " at frame " frame > "/dev/stderr"
            failed = 1
        }
        /^TEST troll-spin state / {
            frame = $4 + 0
            seen[frame]++
            for (i = 5; i <= NF; ++i) {
                split($i, pair, "=")
                state[pair[1]] = pair[2] + 0
            }
            if (frame >= 207 && frame <= 344) {
                if (!state["present"] || !state["spin"] || state["deadline"] != 344 ||
                    !state["targetable"] || state["dying"] || state["hp"] != 75 ||
                    state["death1"] || state["death2"])
                    fail("changed during rampage")
                if (state["shot"] != expectedShot[frame] || state["unit"] != expectedUnit[frame] ||
                    state["building"] != expectedBuilding[frame])
                    fail("weapon cadence or damage differed")
            }
            if (frame == 345) {
                if (!state["present"] || state["spin"] || state["deadline"] ||
                    state["targetable"] || !state["dying"] || state["hp"] ||
                    state["death1"] + state["death2"] != 1 ||
                    state["unit"] != 1200 || state["building"] != 1200)
                    fail("failed to enter slow death at the rampage deadline")
                corpseDeadline = state["death1"] ? 944 : 1394
            }
            if (frame == 358) {
                if (!state["spin"] || state["hp"] != 75)
                    fail("vulnerable control did not start rampage")
            }
            if (frame == 359 || frame == 361) {
                if (!state["present"] || state["spin"] || state["targetable"] || state["hp"] ||
                    state["death1"] + state["death2"] != 1)
                    fail("lethal hit did not kill the control")
            }
            if (corpseDeadline && (frame == 944 || frame == 945 || frame == 1394 || frame == 1395)) {
                if (state["present"] != (frame <= corpseDeadline) || state["unit"] != 1200 || state["building"] != 1200)
                    fail("corpse duration or post-death damage differed")
            }
        }
        END {
            for (frame = 207; frame <= 345; ++frame)
                if (frame >= first && frame <= end && seen[frame] != 1)
                    fail("missing or duplicate trace")
            split("358 359 361 944 945 1394 1395", samples)
            for (i in samples) {
                frame = samples[i] + 0
                if (frame >= first && frame <= end && seen[frame] != 1)
                    fail("missing or duplicate control or corpse sample")
            }
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '/^TEST troll-spin / && $4 >= first' "$out/$phase.log" > "$out/troll-spin-$phase.txt"
    done
    diff -u "$out/troll-spin-continue.txt" "$out/troll-spin-resume.txt"
}
