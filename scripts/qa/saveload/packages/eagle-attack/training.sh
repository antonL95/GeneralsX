prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 185; frame <= 370 && frame <= end; ++frame) {
            print frame, "eagle-trace Player_1 EagleAttackBird 0 PlyrCreeps:EagleAttackTarget:0"
            print frame, "air-swoop-trace Player_1 EagleAttackBird 0"
        }
        for (frame = 447; frame <= 632 && frame <= end; ++frame)
            print frame, "eagle-trace Player_1 EagleAttackBird 0 PlyrCreeps:EagleAttackOrc:0"
    }'
}

check_moving_contact() {
    awk '
        /^TEST eagle / && substr($3, 7) + 0 <= 370 && !shot {
            for (i = 5; i <= NF; ++i)
                if ($i ~ /^fire0=/)
                    shot = substr($i, 7) + 0
        }
        /^TEST air-swoop .* actor=EagleAttackBird / {
            for (i = 5; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            frame = substr($3, 7) + 0
            speedSquared[frame] = sample["vx"] ^ 2 + sample["vy"] ^ 2
        }
        END {
            printf "eagle moving contact shot=%u speed before=%.3f at=%.3f after=%.3f\n",
                shot, sqrt(speedSquared[shot - 1]), sqrt(speedSquared[shot]), sqrt(speedSquared[shot + 1])
            exit !(shot && speedSquared[shot - 1] > 0 && speedSquared[shot] > 0 && speedSquared[shot + 1] > 0)
        }
    ' "$log"
}

check() {
    if [ "$phase" = continue ]; then
        grep -E "^TEST eagle frame=$save_frame .*swoop=0 .*target=EagleAttackTarget .*carrier=EagleAttackBird .*fire0=[1-9][0-9]* " "$log"
    fi
    if ! covers 370; then
        return
    fi
    if [ "$phase" = continue ]; then
        check_moving_contact || return
        awk '
        /^TEST eagle / && substr($3, 7) + 0 <= 370 {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            if (sample["victim"] != "-" && sample["victim"] != "EagleAttackTarget") {
                printf "eagle acquired %s at frame %u\n", sample["victim"], sample["frame"]
                interference = 1
            }
            if (sample["fire0"] > 0 && !shot) {
                shot = sample["fire0"] + 0
                contact = shot == frame && swoop == 1 && inRange == 1 && sample["frame"] == shot + 1
                damage = hp > sample["hp"] && sample["hp"] > 0
                grab = sample["carrier"] == "EagleAttackBird"
            }
            if (grab && sample["frame"] > shot + 1 && sample["carrier"] == "-" && sample["hp"] > 0)
                released = 1
            frame = sample["frame"] + 0
            swoop = sample["swoop"] + 0
            inRange = sample["in-range"] + 0
            hp = sample["hp"] + 0
        }
        END {
            printf "eagle shot=%u contact=%u damage=%u grab=%u released=%u\n", shot, contact, damage, grab, released
            exit !(contact && damage && grab && released && !interference)
        }
        ' "$log" || return
    fi
    if ! covers 632; then
        return
    fi
    awk '
        /^TEST eagle / && substr($3, 7) + 0 >= 447 {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            if (sample["victim"] == "EagleAttackOrc" && sample["carrier"] == "EagleAttackOrcHorde" &&
                sample["swoop"] == 1 && !sample["fire0"] && !sample["fire2"])
                selected = sample["slot"]
            if (sample["fire2"] && !shot)
                plow = 1
            if (sample["fire0"] && !shot && !plow) {
                shot = sample["fire0"] + 0
                if (sample["target"] == "EagleAttackOrc" && sample["carrier"] == "EagleAttackBird" && sample["hp"] > 0)
                    grabbed = sample["frame"] + 0
            }
            if (grabbed && !released && sample["target"] == "EagleAttackOrc" && sample["carrier"] == "-" && sample["hp"] > 0)
                released = sample["frame"] + 0
        }
        END {
            printf "eagle horde slot=%s plow-first=%u grab-shot=%u contained=%u released=%u\n", selected, plow, shot, grabbed, released
            exit !(selected == "0" && !plow && shot && grabbed && released)
        }
    ' "$log"
}

compare() {
    for eagle_phase in continue resume; do
        awk -v frame="$save_frame" '/^TEST eagle frame=/ && substr($3, 7) + 0 > frame' \
            "$out/$eagle_phase.log" > "$out/eagle-$eagle_phase.trace"
        awk -v frame="$save_frame" '
            /^TEST air-swoop .* actor=EagleAttackBird / && substr($3, 7) + 0 > frame
        ' "$out/$eagle_phase.log" > "$out/eagle-flight-$eagle_phase.trace"
    done
    cmp "$out/eagle-continue.trace" "$out/eagle-resume.trace"
    cmp "$out/eagle-flight-continue.trace" "$out/eagle-flight-resume.trace"
}
