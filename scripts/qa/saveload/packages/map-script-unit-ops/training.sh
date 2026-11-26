check() {
    for frame in 240 246; do
        if covers "$frame"; then
            expect "TEST map-unit-ops frame=$frame attack=1 missing=1"
        fi
    done
}
