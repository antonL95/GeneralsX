check() (
    if covers 472; then
        grep -E '^TEST pkg61 range frame=472 weapon=GarrisonRangeTestBow .*range=197\.50 ' "$log" | awk 'END {exit (NR != 2)}' || return 1
    fi
    if covers 505; then
        grep -E '^TEST pkg61 range frame=505 weapon=GarrisonRangeTestBow .*range=247\.50 ' "$log" || return 1
        grep -E '^TEST pkg61 range frame=505 weapon=GarrisonRangeTestBow .*range=220\.00 ' "$log" || return 1
    fi
)
