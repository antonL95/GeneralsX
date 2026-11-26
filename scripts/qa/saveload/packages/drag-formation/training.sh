check() {
    covers 700 || return 0
    awk '
        $1 == "TEST" && $2 == "drag-formation-state" && $3 == 700 {
            n++
            x = $6; y = $7; facing = $8
            printf "drag-formation %s %s at %.1f %.1f facing %.2f\n", $4, $5, x, y, facing
            if (facing < 1.47 || facing > 1.67) bad = 1
            if ($4 == "DragFormationBow") {
                if (near(x, 1950) && near(y, 3001)) bow = 1
            } else {
                if (near(x, 2002) && near(y, 3075)) left = 1
                if (near(x, 1898) && near(y, 3075)) right = 1
            }
        }
        function near(a, b) { return a - b < 15 && b - a < 15 }
        END {
            if (n != 3 || bad || !bow || !left || !right) {
                print "drag-formation: expected two foot hordes at (1898,3075) and (2002,3075), the archers at (1950,3001), all facing 1.57"
                exit 1
            }
        }
    ' "$log"
}
