prepare() {
    frame=735
    while [ "$frame" -le 1195 ] && [ "$frame" -le "$end" ]; do
        printf '%s horde-speed Player_1 GondorKnightHorde 0\n' "$frame"
        printf '%s horde-speed Player_1 GondorFighterHorde 0\n' "$frame"
        frame=$((frame + 5))
    done
}

check() {
    covers 1195 || return 0
    awk '
        $1 == "TEST" && $2 == "horde-speed" {
            frame = $3; name = $4
            if (name == "GondorKnightHorde") { last = 1060 } else { last = 1190 }
            if (frame >= 800 && frame <= last) {
                if (!(name in from)) { from[name] = frame; x[name] = $5; y[name] = $6; low[name] = 1e9 }
                to[name] = frame; endx[name] = $5; endy[name] = $6; authored[name] = $8
                if ($7 < low[name]) low[name] = $7
                if ($10 > worst[name]) worst[name] = $10
            }
        }
        END {
            for (name in from) {
                seen++
                average = sqrt((endx[name] - x[name]) ^ 2 + (endy[name] - y[name]) ^ 2) / (to[name] - from[name]) * 30
                printf "%s average=%.1f authored=%.1f lowest=%.1f worstslot=%.1f frames=%d-%d\n", name, average, authored[name], low[name], worst[name], from[name], to[name]
                if (average < 0.95 * authored[name] || low[name] < 0.95 * authored[name] || worst[name] > 30) failed = 1
            }
            exit (seen != 2 || failed)
        }
    ' "$log"
}
