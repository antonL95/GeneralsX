check() (
    if covers 216; then
        grep -E '^TEST pkg61 range frame=216 weapon=NormalMeleeHordeRangefinder ' "$log" || return 1
    fi
    if covers 240; then
        grep -E '^TEST pkg61 range frame=240 weapon=NormalMissileHordeRangefinder ' "$log" || return 1
    fi
    if covers 250; then
        grep -E '^TEST pkg61 range frame=250 weapon=NormalMissileHordeRangefinder ' "$log" || return 1
    fi
    if covers 262; then
        grep -E '^TEST pkg61 range frame=262 weapon=NormalMeleeHordeRangefinder ' "$log" || return 1
    fi
)
