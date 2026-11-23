check() {
    for frame in 230 237 245 251; do
        covers "$frame" || continue
        primary=15
        alternate=0
        active=0
        if [ "$frame" -eq 237 ] || [ "$frame" -eq 245 ]; then
            primary=0
            alternate=15
            active=1
        fi
        expect "TEST formation-state $frame FormationButtonsGuard 0 members=15 primary=$primary alternate=$alternate active=$active"
        expect "TEST formation-state $frame FormationButtonsPikes 0 members=15 primary=$primary alternate=$alternate active=$active"
        expect "TEST formation-state $frame FormationButtonsControl 0 members=15 primary=15 alternate=0 active=0"
    done
}
