prepare() {
    printf '%s deposit-xp-load-levels - - 0 %s/levels.ini\n' 100 "$package_dir" "$save_frame" "$package_dir"
}

check() {
    covers 262 && covers 500 || return 0
    awk '
        /^TEST deposit-xp / {
            for (i = 3; i <= NF; i++) {
                split($i, pair, "=")
                row[pair[1]] = pair[2]
            }
            cash[row["label"]] = row["cash"]
            xp[row["label"]] = row["xp"]
        }
        END {
            gained = cash["rounding-b"] - cash["rounding-a"]
            experience = xp["rounding-b"] - xp["rounding-a"]
            if (gained != 10 || experience < 9.75 || experience > 9.77) {
                printf "Expected a delivery of 10 with 9.76 experience, got %s and %s\n", gained, experience
                exit 1
            }
            print "Delivery rounded up to 10 with unrounded experience " experience
        }
    ' "$log"
}
