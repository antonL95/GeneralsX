check() {
    for frame in 239 248; do
        if covers "$frame"; then
            expect "TEST map-unit-ops xp frame=$frame sink=1 amount=1 negative=1"
        fi
    done
    for frame in 241 247; do
        if covers "$frame"; then
            expect "TEST map-unit-ops teleport frame=$frame moved=1 missing=1"
        fi
    done
    for frame in 240 246; do
        if covers "$frame"; then
            expect "TEST map-unit-ops frame=$frame attack=1 missing=1"
        fi
    done
}
