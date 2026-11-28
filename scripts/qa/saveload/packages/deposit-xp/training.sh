prepare() {
    printf '%s deposit-xp-load-levels - - 0 %s/levels.ini\n' 100 "$package_dir" "$save_frame" "$package_dir"
}

delta() {
    awk -v first="$1" -v second="$2" -v field="$3" -v low="$4" -v high="$5" '
        /^TEST deposit-xp / {
            for (i = 3; i <= NF; i++) {
                split($i, pair, "=")
                row[pair[1]] = pair[2]
            }
            for (key in row)
                value[row["label"], key] = row[key]
            seen[row["label"]] = 1
        }
        END {
            if (!(first in seen) || !(second in seen)) {
                print "Missing deposit-xp samples " first " and " second
                exit 1
            }
            change = value[second, field] - value[first, field]
            if (change < low - 0.001 || change > high + 0.001) {
                printf "Expected %s change %s to %s between %s and %s, got %s\n", field, low, high, first, second, change
                exit 1
            }
            printf "%s %s to %s changed by %s\n", field, first, second, change
        }
    ' "$log"
}

check() {
    if covers 1261 && covers 1279; then
        delta plain-a plain-b cash 30 30 || return 1
        delta plain-a plain-b xp 30 30 || return 1
        expect_rank plain-b 2 || return 1
    fi
    if covers 1281 && covers 1299; then
        delta plain-c plain-d cash 21 21 || return 1
        delta plain-c plain-d xp 22.5 22.5 || return 1
    fi
    if covers 1301 && covers 1319; then
        delta plain-e plain-f cash 0 0 || return 1
        delta plain-e plain-f xp 0 0 || return 1
    fi
    if covers 1323 && covers 1341; then
        delta noxp-a noxp-b cash 30 30 || return 1
        delta noxp-a noxp-b xp 0 0 || return 1
    fi
    if covers 1346 && covers 1365; then
        delta garrison-a garrison-b cash 0 0 || return 1
    fi
    if covers 1366 && covers 1425; then
        delta garrison-c garrison-d cash 30 100 || return 1
        grep -E '^TEST deposit-xp frame=1425 label=garrison-d .* contained=1$' "$log" || return 1
    fi
    return 0
}

expect_rank() {
    grep -E "^TEST deposit-xp frame=[0-9]+ label=$1 rank=$2 " "$log" || {
        printf 'Expected rank %s at %s\n' "$2" "$1" >&2
        return 1
    }
}
