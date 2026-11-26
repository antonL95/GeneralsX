check() {
    for frame in 242 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops threat frame=$frame match=1 reject=1 cached=1"
        fi
    done
    for frame in 243 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops health frame=$frame partial=1 match=1 reject=1"
        fi
    done
    for frame in 240 248; do
        if covers "$frame"; then
            expect "TEST map-unit-ops team-xp frame=$frame sink=1 members=1"
        fi
    done
    for frame in 243 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops veterancy frame=$frame match=1 reject=1"
        fi
    done
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
