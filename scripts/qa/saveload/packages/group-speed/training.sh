check() {
    if ! covers 236 || ! covers 391; then
        return 0
    fi
    awk '
        $1 == "TEST" && $2 == "track" && $3 == 236 { name[$4] = $5 }
        $1 == "TEST" && $2 == "pos" && $5 == 0 && ($4 in name) {
            if ($3 == 240) start[name[$4]] = $6
            if ($3 == 390) late[name[$4]] = $6
        }
        END {
            foot = late["GroupSpeedFoot"] - start["GroupSpeedFoot"]
            rider = late["GroupSpeedRider"] - start["GroupSpeedRider"]
            printf "group-speed progress after 150 frames: foot %.1f rider %.1f\n", foot, rider
            if (rider < 375 || foot < 150) {
                print "group-speed: expected the rider at its own speed (100) and the foot horde near 50"
                exit 1
            }
        }
    ' "$log"
}
