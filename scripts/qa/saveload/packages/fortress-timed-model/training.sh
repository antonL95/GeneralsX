check() {
    if covers 55; then
        expect "TEST conditions 55 FortressTimingFixture ivory=0 stonework=0"
    fi
    if covers 60; then
        expect "FORTRESS order 60 FortressTimingFixture Command_FortressTimingFixture Upgrade_FortressTimingFixture available=1 frames=30"
    fi
    for frame in 120 244 245; do
        if covers "$frame"; then
            expect "TEST conditions $frame FortressTimingFixture ivory=1 stonework=1"
        fi
    done
    if covers 310; then
        expect "TEST conditions 310 FortressTimingFixture ivory=1 stonework=0"
    fi
    if covers 365; then
        expect "TEST conditions 365 FortressTimingFixture ivory=0 stonework=0"
    fi
}
