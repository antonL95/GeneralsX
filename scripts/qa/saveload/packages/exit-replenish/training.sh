prepare() {
    [ "$end" -ge 2000 ] || return 0
    awk -v end="$end" 'BEGIN {
        for (frame = 300; frame <= end; frame += 20)
            print frame, "exit-stuck-sample Player_1 HarondorOutpost 0 -"
    }'
}

check() {
    covers 2000 || return 0
    awk '
        function field(name,    i, parts) {
            for (i = 4; i <= NF; ++i) {
                split($i, parts, "=")
                if (parts[1] == name) return parts[2]
            }
            return ""
        }
        $1 == "TEST" && $2 == "exit-stuck-anchor" {
            ++samples
            if (field("unslotted") > 0) {
                printf "%d horde %s has %s members without a slot\n", $3, $5, field("unslotted")
                failed = 1
            }
        }
        $1 == "TEST" && $2 == "exit-stuck" && $3 == 2000 {
            ++settled
            if (field("slotdist") > 60) { printf "%s far from its slot (%s)\n", $5, field("slotdist"); failed = 1 }
        }
        END {
            if (!samples || !settled) { print "no exit-stuck samples"; failed = 1 }
            exit failed
        }
    ' "$log"
}
