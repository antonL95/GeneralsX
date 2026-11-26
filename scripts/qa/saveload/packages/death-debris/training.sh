check() {
    if covers 280; then
        expect 'TEST death-debris frame=280 template=DeathDebrisSpawn count=1' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisPiece count=0' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisConverted count=1' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisConversionVictim count=0' || return 1
    fi
}

compare() {
    for phase in continue resume; do
        grep '^TEST death-debris ' "$out/$phase.log" > "$out/death-debris-$phase.txt"
    done
    diff -u "$out/death-debris-continue.txt" "$out/death-debris-resume.txt"
}
