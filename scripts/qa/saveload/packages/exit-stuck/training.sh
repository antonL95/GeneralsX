prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 1200; frame <= end && frame <= 2380; frame += 20) {
            print frame, "exit-stuck-sample Player_1 ElvenBarracks 0 -"
            print frame, "exit-stuck-sample Player_1 ElvenBarracks 1 -"
        }
    }'
}

check() {
    return 0
}
