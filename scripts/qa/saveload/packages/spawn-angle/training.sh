fixture_present() {
    grep -q 'construct Player_1 SpawnAnglePorter 0 SpawnAngleMordorFoundation' "$out/commands.txt"
}

prepare() {
    fixture_present || return 0
    printf '400 spawn-angle-state Player_1 MordorFortressCitadel 0\n'
    printf '401 spawn-angle-state Player_1 MirkwoodCitadel 0\n'
    for frame in $(seq 1295 "$end"); do
        printf '%s spawn-angle-state Player_1 MordorFortressCitadel 0\n' "$frame"
        printf '%s spawn-angle-state Player_1 MirkwoodCitadel 0\n' "$frame"
    done
}

check() {
    fixture_present || return 0
    if [ "$phase" = continue ]; then
        expect 'FORTRESS order 400 MordorFortressCitadel Command_PurchaseUpgradeMordorFortressFireArrows Upgrade_MordorFortressFireArrowsTrigger available=1 frames=900'
        expect 'FORTRESS order 401 MirkwoodCitadel Command_PurchaseUpgradeMirkwoodFortressGarrison Upgrade_MirkwoodFortressGarrison available=1 frames=900'
    fi

    awk -v phase="$phase" -v first="$first" -v end="$end" '
        BEGIN {
            mordor = "MordorFortressCitadel"
            slots[mordor] = 1
            slots["MirkwoodCitadel"] = 8
            order[mordor] = 400
            order["MirkwoodCitadel"] = 401
            tau = 8 * atan2(1, 1)
            turn_limit = tau / (1.5 * 30)
            angle_tolerance = 0.001
        }
        function fail(message, frame, name) {
            print phase, name, "frame", frame ":", message
            failed = 1
        }
        function absolute(value) {
            return value < 0 ? -value : value
        }
        function check_flight(frame, angle_delta, distance_squared, turn) {
            if (frame == birth[mordor]) {
                if (phase == "continue" && (absolute(angle_delta) > angle_tolerance || distance_squared > 1))
                    fail("Fellbeast did not start at the authored pose", frame, mordor)
            } else {
                turn = angle_delta - previous_angle
                if (turn > tau / 2) turn -= tau
                if (turn < -tau / 2) turn += tau
                if (absolute(turn) > turn_limit + angle_tolerance)
                    fail("Fellbeast snapped its heading", frame, mordor)
                if (absolute(turn) > angle_tolerance) {
                    if (!flight_turns && phase == "continue" && turn < 0)
                        fail("first idle wing turn should be positive", frame, mordor)
                    flight_turns++
                }
            }
            if (distance_squared > 1) flew = 1
            previous_angle = angle_delta
        }
        /^SPAWN_ANGLE / {
            frame = $2
            name = $3
            count = $4
            if (frame == order[name] && phase == "continue") {
                baseline[name]++
                if (count != 0)
                    fail("upgrade payload already present", frame, name)
            } else if (frame >= 1295) {
                samples[name]++
                if (count && !birth[name]) {
                    birth[name] = frame
                    identity[name] = $7
                }
                if (birth[name] && (count != slots[name] || $5 != count || $7 != identity[name]))
                    fail($0, frame, name)
                if (name == mordor && count == 1)
                    check_flight(frame, $8, $9)
            }
            if ($6 != 1)
                fail("expected a rotated citadel", frame, name)
        }
        END {
            for (name in slots) {
                start = first > 1295 ? first : 1295
                if (samples[name] != end - start + 1)
                    fail("missing observations", end, name)
                if (phase == "continue" && (baseline[name] != 1 || birth[name] < order[name] + 900 || end - birth[name] < 90))
                    fail("expected spawn after research and at least 90 idle frames", birth[name], name)
                if (phase == "resume" && birth[name] != first)
                    fail("payload missing on first resumed frame", first, name)
            }
            if (!flight_turns || !flew)
                fail("expected idle circling", end, mordor)
            if (!failed)
                printf "Spawn angle %s: Fellbeast circled from sample %d and all %d Mirkwood slots held from sample %d\n", phase, birth[mordor], slots["MirkwoodCitadel"], birth["MirkwoodCitadel"]
            exit failed
        }
    ' "$log"
}
