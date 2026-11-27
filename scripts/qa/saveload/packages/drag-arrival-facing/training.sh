check() {
    covers 257 || return 0
    awk '
        $1 == "TEST" && $2 == "drag-formation-state" && $4 == "DragArrivalCw" && $3 == 253 {
            cwStartSeen = 1
            if ($9 != 0) cwStartBad = 1
        }
        $1 == "TEST" && $2 == "drag-formation-state" && $4 == "DragArrivalCw" && $3 == 254 {
            cwSnapSeen = 1
            if ($9 != 1 || $8 > -1.47 || $8 < -1.67) cwSnapBad = 1
        }
        $1 == "TEST" && $2 == "drag-formation-state" && $4 == "DragArrivalCcw" && $3 == 256 {
            ccwTurnSeen = 1
            if ($9 != 0 || $8 < 1.30 || $8 > 1.50) ccwTurnBad = 1
        }
        $1 == "TEST" && $2 == "drag-formation-state" && $4 == "DragArrivalCcw" && $3 == 257 {
            ccwSnapSeen = 1
            if ($9 != 1 || $8 < 1.60 || $8 > 1.70) ccwSnapBad = 1
        }
        END {
            if (!cwStartSeen || cwStartBad || !cwSnapSeen || cwSnapBad || !ccwTurnSeen || ccwTurnBad || !ccwSnapSeen || ccwSnapBad) {
                printf "drag-arrival-facing failed: cwStart=%d/%d cwSnap=%d/%d ccwTurn=%d/%d ccwSnap=%d/%d\n",
                    cwStartSeen, cwStartBad, cwSnapSeen, cwSnapBad, ccwTurnSeen, ccwTurnBad, ccwSnapSeen, ccwSnapBad
                exit 1
            }
        }
    ' "$log"
}
