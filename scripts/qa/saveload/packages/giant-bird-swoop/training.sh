prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 185; frame <= 430 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestEagle 0 PlyrCreeps:SwoopTestTarget:0"
        for (frame = 225; frame <= 419 && frame <= end; ++frame)
            print frame, "giant-bird-route-trace Player_1 SwoopTestRouteBird 0"
        for (frame = 507; frame <= 752 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestFellBeast 0 PlyrCreeps:SwoopTestTarget:0"
        for (frame = 829; frame <= 1040 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestEagle 0 PlyrCreeps:GondorTrebuchetDebrisPiece:0"
    }'
}

check_swoop_toss() {
    local label="$1" bird="$2" start_frame="$3" end_frame="$4"
    awk -v label="$label" -v bird="$bird" -v start_frame="$start_frame" -v end_frame="$end_frame" '
        /^TEST giant-bird-swoop / {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            frame = sample["frame"] + 0
            if (frame < start_frame || frame > end_frame)
                next
            if (sample["swoop"] == "1" && !dive_start)
                dive_start = frame
            if (sample["shot"] + 0 > 0 && !shot)
                shot = sample["shot"] + 0
            if (sample["carrier"] == bird && !grab_frame)
                grab_frame = frame
            if (grab_frame && !release_frame && sample["carrier"] == "-") {
                release_frame = frame
                release_freefall = sample["freefall"] + 0
                release_impact = sample["impact"] + 0
                release_dead = sample["dead"] + 0
                release_tvx = sample["tvx"] + 0
                release_tvy = sample["tvy"] + 0
                release_tvz = sample["tvz"] + 0
                release_tz = sample["tz"] + 0
            }
            if (release_frame && frame > release_frame && !landed_frame && sample["dead"] == "1") {
                landed_frame = frame
                landed_splatted = sample["splatted"] + 0
            }
        }
        END {
            printf "%s dive=%u shot=%u grab=%u release=%u freefall=%u impact=%u release_dead=%u tv=(%.3f,%.3f,%.3f) tz=%.3f landed_dead=%u splatted=%u\n",
                label, dive_start, shot, grab_frame, release_frame, release_freefall, release_impact, release_dead,
                release_tvx, release_tvy, release_tvz, release_tz, landed_frame, landed_splatted
            ok = dive_start > 0 && shot > dive_start && grab_frame > 0 && release_frame > grab_frame &&
                release_freefall == 1 && release_impact == 1 && release_dead == 0 &&
                release_tvx > 0 && release_tvz < 0 && landed_frame > release_frame && landed_splatted == 1
            exit !ok
        }
    ' "$log"
}

check_route() {
    awk -v save_frame="$save_frame" '
        /^TEST giant-bird-route / {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2] + 0
            }
            if (!started) {
                initial_ground = sample["ground"]
                started = 1
            }
            if (sample["frame"] == save_frame)
                saved_in_motion = sample["distance"] > 0 && sample["speed"] > 0
            if (sample["next"] != previous_update && sample["slope"] > 0.002 && sample["slope"] <= 1.0 &&
                sample["ground"] > initial_ground && sample["distance"] > previous_distance) {
                expected = sample["limit"] * (1.0 - 0.4 * sample["slope"])
                error = sample["speed"] - expected
                if (error >= -0.0001 && error <= 0.0001 && !uphill_frame) {
                    uphill_frame = sample["frame"]
                    uphill_speed = sample["speed"]
                    uphill_slope = sample["slope"]
                }
            }
            previous_update = sample["next"]
            previous_distance = sample["distance"]
            if (sample["ground"] > highest_ground)
                highest_ground = sample["ground"]
            if (sample["y"] >= 2238)
                arrived = 1
        }
        END {
            printf "bird route active at save=%u uphill frame=%u slope=%.5f speed=%.5f ground=%.3f to %.3f arrival=%u\n",
                saved_in_motion, uphill_frame, uphill_slope, uphill_speed, initial_ground, highest_ground, arrived
            exit !(saved_in_motion && uphill_frame && highest_ground > initial_ground && arrived)
        }
    ' "$log"
}

check() {
    if [ "$phase" = continue ] && covers 419; then
        check_route || return
    fi
    if covers 185 && covers 430; then
        check_swoop_toss "eagle" "SwoopTestEagle" 185 430 || return
    fi
    if covers 507 && covers 752; then
        check_swoop_toss "fellbeast" "SwoopTestFellBeast" 507 752 || return
    fi
    if covers 1040; then
        awk '
            /^TEST giant-bird-swoop / {
                for (i = 3; i <= NF; ++i) {
                    split($i, field, "=")
                    sample[field[1]] = field[2]
                }
                frame = sample["frame"] + 0
                if (frame < 829 || frame > 1040)
                    next
                if (sample["passenger"] == "GondorTrebuchetDebrisPiece" && !debris_grab)
                    debris_grab = frame
                if (debris_grab && !debris_release && sample["contain"] == "0" && sample["target"] == "GondorTrebuchetDebrisPiece") {
                    debris_release = frame
                    debris_freefall = sample["freefall"] + 0
                    debris_tvz = sample["tvz"] + 0
                }
            }
            END {
                printf "debris grab=%u release=%u freefall=%u tvz=%.3f\n",
                    debris_grab, debris_release, debris_freefall, debris_tvz
                exit !(debris_grab > 0 && debris_release > debris_grab && debris_freefall == 1 && debris_tvz < 0)
            }
        ' "$log" || return
    fi
}

compare() {
    for phase_name in continue resume; do
        awk -v frame="$save_frame" '/^TEST giant-bird-swoop frame=/ && substr($3, 7) + 0 > frame' \
            "$out/$phase_name.log" > "$out/giant-bird-swoop-$phase_name.trace"
    done
    cmp "$out/giant-bird-swoop-continue.trace" "$out/giant-bird-swoop-resume.trace" || return
    for phase_name in continue resume; do
        awk -v frame="$save_frame" '/^TEST giant-bird-route frame=/ && substr($3, 7) + 0 > frame' \
            "$out/$phase_name.log" > "$out/giant-bird-route-$phase_name.trace"
    done
    cmp "$out/giant-bird-route-continue.trace" "$out/giant-bird-route-resume.trace"
}
