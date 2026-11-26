prepare() {
    [ "$end" -ge 1450 ] || return 0
    awk -v end="$end" 'BEGIN {
        for (frame = 1120; frame <= end && frame <= 1450; frame += 30)
            print frame, "outrider-exit-sample Player_1 MirkwoodTrainingGrounds 0 -"
    }'
}

check() {
    covers 1450 || return 0
    awk '
        function field(name,    i, parts) {
            for (i = 4; i <= NF; ++i) {
                split($i, parts, "=")
                if (parts[1] == name) return parts[2]
            }
            return ""
        }
        $1 == "TEST" && $2 == "outrider-exit-anchor" && $3 == 1450 {
            anchors++
            members = field("members") + 0
            moved = field("moved") + 0
            if (members < 10) { printf "frame 1450 expected 10 members, got %d\n", members; failed = 1 }
            if (moved < 200) { printf "frame 1450 horde did not move away (%.1f)\n", moved; failed = 1 }
        }
        $1 == "TEST" && $2 == "outrider-exit" && $3 == 1450 {
            settled++
            state = field("state") + 0
            slotdist = field("slotdist") + 0
            if (state == 7) { printf "frame 1450 member %s still on exit path (slotdist=%.1f)\n", $4, slotdist; failed = 1 }
            if (slotdist > 60) { printf "frame 1450 member %s far from slot (%.1f)\n", $4, slotdist; failed = 1 }
        }
        END {
            if (!anchors || settled < 10) { printf "incomplete outrider-exit samples (anchors=%d settled=%d)\n", anchors + 0, settled + 0; failed = 1 }
            exit failed
        }
    ' "$log"
}
