prepare() {
    frame=20
    while [ "$frame" -le 700 ] && [ "$frame" -le "$end" ]; do
        printf '%s yield-stall-probe Player_1 YieldStallGroupCavalry 0 805,2505\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallGroupInfantry 0 805,2655\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallGroupSingle 0 800.5,2700.5\n' "$frame"
        frame=$((frame + 5))
    done
}

check() {
    covers 700 || return 0
    awk '
        $1 == "TEST" && $2 == "yield-stall" && $3 == 700 {
            split($7, anchor, "="); split($8, slot, "="); split($9, far, "=")
            seen++
            if (anchor[2] > 30 || slot[2] > 80 || far[2] > 90 || $11 != "idleshort=0") {
                printf "%s stalled at frame 700: %s %s %s %s\n", $4, $7, $8, $9, $11
                failed = 1
            }
        }
        END { exit (seen != 3 || failed) }
    ' "$log"
}
