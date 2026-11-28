prepare() {
    frame=731
    while [ "$frame" -le 1195 ] && [ "$frame" -le "$end" ]; do
        printf '%s horde-speed Player_1 GondorKnightHorde 0\n' "$frame"
        printf '%s horde-speed Player_1 GondorFighterHorde 0\n' "$frame"
        frame=$((frame + 1))
    done
}

check() {
    covers 1195 || return 0
    awk '
        $1 == "TEST" && $2 == "horde-speed" {
            frame = $3; name = $4; speed = $7; auth = $8; members = $9; slot = $10
            if (name == "GondorKnightHorde") { settle = 46; span = 280; fixed = 780; latest = 737 } else { settle = 25; span = 430; fixed = 760; latest = 738 }
            if (!(name in start)) {
                if (speed == 0) stopped[name] = 1
                else start[name] = (name in stopped) ? frame + settle : fixed
                if (speed != 0 && (name in stopped) && frame > latest) {
                    printf "%s first_moving=%d limit=%d\n", name, frame, latest
                    failed = 1
                }
                if (name in start) last[name] = start[name] + span
            }
            if (frame == 745 && name == "GondorKnightHorde") {
                knights_turn_seen = 1
                knights_turn_speed = speed
                knights_turn_auth = auth
            }
            if ((name in start) && frame >= start[name] && frame <= last[name]) {
                if (!(name in from)) {
                    from[name] = frame; x[name] = $5; y[name] = $6
                    low[name] = 1e9; min_members[name] = 1e9
                }
                to[name] = frame; endx[name] = $5; endy[name] = $6; authored[name] = auth
                if (speed < low[name]) low[name] = speed
                if (members < min_members[name]) min_members[name] = members
                if (frame == last[name]) endslot[name] = slot
            }
        }
        END {
            if (knights_turn_seen && knights_turn_speed > 0.11 * knights_turn_auth) {
                printf "GondorKnightHorde turn_speed=%.1f limit=%.1f\n", knights_turn_speed, 0.11 * knights_turn_auth
                failed = 1
            }
            for (name in from) {
                seen++
                average = sqrt((endx[name] - x[name]) ^ 2 + (endy[name] - y[name]) ^ 2) / (to[name] - from[name]) * 30
                printf "%s average=%.1f authored=%.1f lowest=%.1f endslot=%.1f members=%d frames=%d-%d\n", name, average, authored[name], low[name], endslot[name], min_members[name], from[name], to[name]
                if (to[name] != last[name] || to[name] <= from[name] || min_members[name] <= 0 || authored[name] <= 0) failed = 1
                if (knights_turn_seen && from[name] != start[name]) failed = 1
                if (average < 0.95 * authored[name] || low[name] < 0.95 * authored[name] || endslot[name] > 5.0) failed = 1
            }
            exit (seen != 2 || failed)
        }
    ' "$log"
}
