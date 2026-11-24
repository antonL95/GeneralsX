prepare() {
    frame=275
    while [ "$frame" -le 310 ]; do
        printf '%s corner-speed Player_1 GaitHorse 0\n' "$frame"
        printf '%s steering-path Player_1 GaitHorse 0\n' "$frame"
        frame=$((frame + 1))
    done
}

check() {
    if covers 310; then
        awk '
            $1 == "TEST" && $2 == "corner-speed" && $4 == "GaitHorse" {
                split($5, value, "=")
                scalar[$3] = value[2] + 0
            }
            $1 == "TEST" && $2 == "steering" && $4 == "GaitHorse" && $3 > 281 {
                split($16, value, "=")
                decel[$3] = value[2] + 0
                print
            }
            END {
                for (frame = 282; frame <= 310; frame++) {
                    if (!(frame in scalar)) continue
                    if (scalar[frame] >= 1.1 && decel[frame] != 0) failed = 1
                    if (scalar[frame] > 0 && scalar[frame] < 1.1) {
                        if (decel[frame] != 1) failed = 1
                        on = 1
                    }
                    if (on && scalar[frame] == 0 && decel[frame] == 0) off = 1
                }
                exit !(on && off && !failed)
            }
        ' "$log" || return 1
    fi
}
