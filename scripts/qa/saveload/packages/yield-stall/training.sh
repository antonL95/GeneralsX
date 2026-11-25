prepare() {
    frame=20
    while [ "$frame" -le 400 ] && [ "$frame" -le "$end" ]; do
        printf '%s yield-stall-probe Player_1 YieldStallCavalry 0 2500,1700\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallInfantry 0 2500,2000\n' "$frame"
        printf '%s yield-stall-probe Player_1 YieldStallSingle 0 2500,2300\n' "$frame"
        frame=$((frame + 5))
    done
}

check() {
    covers 400 || return 0
    awk '
        $1 == "TEST" && $2 == "yield-stall" && $3 == 400 {
            split($6, anchor, "="); split($7, slot, "="); split($8, far, "=")
            seen++
            if (anchor[2] > 60 || slot[2] > 60 || far[2] > 140 || $10 != "idleshort=0") {
                printf "%s stalled at frame 400: %s %s %s %s\n", $4, $6, $7, $8, $10
                failed = 1
            }
        }
        END { exit (seen != 3 || failed) }
    ' "$log"
}
