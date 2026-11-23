prepare() {
    for frame in $(seq 183 255); do
        printf '%s log Player_1 CollapseFixtureSink 0\n' "$frame"
    done
}

check() {
    awk -v first="$first" -v end="$end" '
        function abs(x) { return x < 0 ? -x : x }
        function covers(f) { return f >= first && f <= end }
        function fail(message) { print message; failed = 1 }
        /^TEST frame [0-9]+: object [0-9]+ CollapseFixtureSink pos / {
            frame = $3 + 0
            elapsed = frame - 183
            expectedZ = 60 - elapsed * (elapsed - 1) * 16 / 900
            if ($8 != 1320 || $9 != 2890 || abs($10 - expectedZ) > 0.002 ||
                $0 !~ /registered 1 dead 1/)
                fail("Unexpected collapse position at frame " frame ": " $0)
            position[frame]++
        }
        /^TEST collapse-phase .* actor=CollapseFixtureSink phase=(ALMOST_FINAL|FINAL) / {
            frame = substr($3, 7) + 0
            split(substr($9, 7), world, ",")
            if ($6 == "phase=ALMOST_FINAL") {
                if ((frame != 243 && frame != 249) || $8 != "ocl=1" ||
                    abs(world[3] - (frame == 243 ? -5.07 : -18.61)) > 0.001)
                    fail("Unexpected late phase: " $0)
            } else if (frame != 254 || abs(world[3] - -30.88) > 0.001)
                fail("Unexpected final phase: " $0)
            phase[frame]++
        }
        /^TEST list 257 Player_1 CollapseFixtureDebris / { debris++ }
        END {
            for (f = 183; f <= 254; f++)
                if (covers(f) && position[f] != 1)
                    fail("Missing or duplicate collapse position at frame " f)
            if ((covers(243) && phase[243] != 1) || (covers(249) && phase[249] != 1) ||
                (covers(254) && phase[254] != 1))
                fail("Expected two authored late OCLs and one final phase across the checkpoint")
            if (covers(257) && debris != 2)
                fail("Expected two debris objects, got " debris + 0)
            exit failed
        }
    ' "$log" || return 1
    if covers 151; then
        expect 'TEST collapse-site frame=151 present=1 registered=1 owned=1 tile=obstacle attackable=1 rubble=0 post=0'
    fi
    for frame in 200 244 254; do
        if covers "$frame"; then
            expect "TEST collapse-site frame=$frame present=1 registered=1 owned=0 tile=rubble attackable=0 rubble=1 post=0"
        fi
    done
    if covers 255; then
        expect 'TEST collapse-site frame=255 present=0 registered=0 owned=0 tile=clear attackable=0 rubble=0 post=0'
        expect 'TEST frame 255: no CollapseFixtureSink 0 of Player_1'
    fi
    if covers 155; then
        grep -Eq '^TEST collapse-phase frame=15[56] .* actor=CollapseFixtureKeep phase=FINAL ' "$log" ||
            { printf 'Missing final phase for retain control\n'; return 1; }
    fi
    if covers 263; then
        grep -Eq '^TEST frame 263: object [0-9]+ CollapseFixtureKeep .* registered 1 dead 1 ' "$log" ||
            { printf 'Missing retained object at frame 263\n'; return 1; }
        expect 'TEST collapse-site frame=263 present=1 registered=1 owned=0 tile=rubble attackable=0 rubble=0 post=1'
    fi
}
