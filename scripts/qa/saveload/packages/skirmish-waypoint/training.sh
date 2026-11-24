check() {
    for frame in 236 242 246; do
        if covers "$frame"; then
            reachable=1
            [ "$frame" -ne 236 ] || reachable=0
            expect "TEST skirmish-waypoint frame=$frame first=1 reachable=$reachable missing=0"
        fi
    done
}
