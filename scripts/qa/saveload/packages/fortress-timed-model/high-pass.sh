check() {
    if covers 58; then
        expect "TEST conditions 58 FortressTimingFixture ivory=0 stonework=0"
    fi
    for frame in 70 80 81; do
        if covers "$frame"; then
            expect "TEST conditions $frame FortressTimingFixture ivory=1 stonework=1"
        fi
    done
    if covers 250; then
        expect "TEST conditions 250 FortressTimingFixture ivory=1 stonework=0"
    fi
    if covers 340; then
        expect "TEST conditions 340 FortressTimingFixture ivory=0 stonework=0"
    fi
}
