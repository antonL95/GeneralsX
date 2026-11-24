prepare() {
    seq 441 649 | awk -v end="$end" '$1 <= end {
        print $1, "maggot-dogs-trace Player_1 ArnorFarmerMaggot 0"
    }'
}

check() {
    if ! covers 649; then
        return 0
    fi
    awk '
        $1 == "TEST" && $2 == "steering" && $3 == 649 && $4 == "ArnorFarmerMaggot" {
            split($5, p, "[=,]")
            mx = p[2]
            my = p[3]
        }
        $1 == "TEST" && $2 == "maggot-dog" && $3 == 519 { startX[$5] = $6 }
        $1 == "TEST" && $2 == "maggot-dog" && $3 == 649 { endX[$5] = $6; endY[$5] = $7 }
        END {
            if (mx < 1550)
                exit 1
            n = 0
            for (dog in endX) {
                n++
                if (endX[dog] - startX[dog] < 40)
                    exit 1
                dx = endX[dog] - mx
                dy = endY[dog] - my
                if (dx * dx + dy * dy > 150 * 150)
                    exit 1
            }
            if (n != 3)
                exit 1
        }
    ' "$log" || {
        printf 'hound-follow: hounds did not follow the walking Maggot\n' >&2
        return 1
    }
}
