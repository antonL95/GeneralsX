prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 185; frame <= 370 && frame <= end; ++frame)
            print frame, "eagle-trace Player_1 GondorGwaihirHero_Summoned 0 PlyrCreeps:EagleAttackTarget:0"
    }'
}

check() {
    if [ "$phase" = continue ]; then
        grep -E "^TEST eagle frame=$save_frame .*swoop=1 .*victim=EagleAttackTarget .*fire0=0 " "$log"
    fi
    if ! covers 370; then
        return
    fi
    awk '
        /^TEST eagle / {
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
                grab = sample["carrier"] == "GondorGwaihirHero_Summoned"
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
    ' "$log"
}

compare() {
    for eagle_phase in continue resume; do
        awk -v frame="$save_frame" '/^TEST eagle frame=/ && substr($3, 7) + 0 > frame' \
            "$out/$eagle_phase.log" > "$out/eagle-$eagle_phase.trace"
    done
    cmp "$out/eagle-continue.trace" "$out/eagle-resume.trace"
}
