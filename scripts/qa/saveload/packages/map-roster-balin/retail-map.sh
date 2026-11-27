check() {
    for frame in 12 24 60; do
        if covers "$frame"; then
            expect "TEST skirmish-catalogue frame=$frame slots=1 high-pass=2 skirmish=1 local=Player_1 start=0 second=0 roster=5"
        fi
    done
}
