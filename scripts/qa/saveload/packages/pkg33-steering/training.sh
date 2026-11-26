check() {
    if covers 15; then
        grep -Eq '^TEST save-load obstacle-count frame=15 template=Pkg33Citadel cells=[1-9][0-9]*$' "$log" || return 1
    fi

    if covers 240; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 240 && $4 ~ /^Pkg33(Guard|Archers|Knights)$/ {
                if ($0 !~ /moving=1/ || $0 !~ /itinerary: [0-9.,]+ [0-9.,]+/)
                    failed = 1
                active[$4] = 1
            }
            END { exit (failed || length(active) != 3) }
        ' "$log" || return 1
    fi

    if covers 1200; then
        awk '
            BEGIN {
                x["Pkg33Guard"] = 1265; y["Pkg33Guard"] = 365
                x["Pkg33Archers"] = 1245; y["Pkg33Archers"] = 455
                x["Pkg33Knights"] = 1295; y["Pkg33Knights"] = 555
            }
            $1 == "TEST" && $2 == "track" && $3 == 246 && ($5 in x) {
                anchor[$4] = $5
            }
            $1 == "TEST" && $2 == "pos" && ($4 in anchor) {
                if ($6 >= 1050 && !($4 in crossing))
                    crossing[$4] = $7 < 400 ? -1 : ($7 > 400 ? 1 : 0)
                if ($3 == 1200 && $9 == 0 && $10 == 0) {
                    name = anchor[$4]
                    dx = $6 - x[name]
                    dy = $7 - y[name]
                    if (dx * dx + dy * dy < 100)
                        arrived[$4] = 1
                }
            }
            END {
                for (id in crossing)
                    sum += crossing[id]
                exit (length(anchor) != 3 || length(arrived) != 3 || (sum != 3 && sum != -3))
            }
        ' "$log" || return 1
    fi
    if covers 1270; then
        awk '
            $1 == "TEST" && $2 == "steering" && $4 ~ /^Pkg33(Guard|Archers|Knights)$/ {
                split($7, goal, /[=,]/)
                split($NF, waypoint, ",")
                goalX = $NF == "itinerary:" ? goal[2] : waypoint[1]
                if ($3 == 1220 && $0 ~ /moving=1/ && goalX < 1100)
                    reversed[$4] = 1
                if ($3 == 1248 && $0 ~ /moving=1/ && goalX > 1300)
                    replaced[$4] = 1
                if ($3 == 1270 && $0 ~ /moving=0/ && $0 ~ /itinerary:$/)
                    stopped[$4] = 1
            }
            END { exit (length(reversed) != 3 || length(replaced) != 3 || length(stopped) != 3) }
        ' "$log"
    fi
}
