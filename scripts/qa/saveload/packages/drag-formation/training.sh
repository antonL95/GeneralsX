check() {
    covers 700 || return 0
    awk '
        $1 == "TEST" && $2 == "drag-formation-state" && $3 == 535 && $4 == "DragFormationBow" {
            turningSeen = 1
            printf "drag-formation turn %s at %.1f %.1f facing %.2f idle %s\n", $4, $6, $7, $8, $9
            if ($9 != 0) turnIdleBad = 1
        }
        $1 == "TEST" && $2 == "drag-formation-state" && $3 == 700 {
            n++
            x = $6; y = $7; facing = $8; idle = $9
            printf "drag-formation %s %s at %.1f %.1f facing %.2f idle %s\n", $4, $5, x, y, facing, idle
            if (idle != 1) bad = 1
            if ($4 == "DragFormationBow") {
                if (facing > -1.47 || facing < -1.67) bad = 1
                if (near(x, 1950) && near(y, 3001)) bow = 1
            } else if ($4 == "DragFormationFoot") {
                if (facing < 1.47 || facing > 1.67) bad = 1
                footX = x; footY = y; foot = 1
            } else if ($4 == "DragFormationWideFoot") {
                if (facing < 1.47 || facing > 1.67) bad = 1
                wideX = x; wideY = y; wide = 1
            }
        }
        function near(a, b) { return a - b < 15 && b - a < 15 }
        function abs(v) { return v < 0 ? -v : v }
        END {
            span = abs(footX - wideX)
            if (!turningSeen || turnIdleBad) {
                print "drag-formation: expected DragFormationBow to remain non-idle while orienting at frame 535"
                exit 1
            }
            if (n != 3 || bad || !bow || !foot || !wide || !near(footY, 3075) || !near(wideY, 3075) || span < 118) {
                printf "drag-formation: expected wide+foot span >= 118 at y=3075 (got %.1f), bow at (1950,3001) facing -1.57\n", span
                exit 1
            }
        }
    ' "$log"
}
