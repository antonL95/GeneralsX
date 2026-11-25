prepare() {
    frame=20
    while [ "$frame" -le 700 ] && [ "$frame" -le "$end" ]; do
        printf '%s yield-stall-probe Player_1 YieldStallCavalryYield 0 2500,2600\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallCavalryAway 0 2500,2900\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallInfantry 0 2500,2000\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallSingle 0 2500,2300\n' "$frame"
        frame=$((frame + 5))
    done
}

check() {
    covers 700 || return 0
    awk '
        $1 == "TEST" && $2 == "yield-stall" && $3 == 700 {
            split($7, anchor, "="); split($8, slot, "="); split($9, far, "=")
            seen++
            if (anchor[2] > 15 || slot[2] > 80 || far[2] > 90 || $11 != "idleshort=0") {
                printf "%s stalled at frame 700: %s %s %s %s\n", $4, $7, $8, $9, $11
                failed = 1
            }
        }
        END { exit (seen != 4 || failed) }
    ' "$log"
}
