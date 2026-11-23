expect_stealth_state() {
    if ! covers "$1"; then
        return 0
    fi
    expect "TEST hobbit-stealth $1 $2 moving=$3 trees=0 legacy=0 stealth=$4 detected=0 hidden=$4 can=0 stealth-model=$4 camo-model=0 hidden-model=$4"
    expect "TEST hobbit-stealth-source $1 $2 ModuleTag_ElvenCamouflage type=0 enabled=1 eligible=0"
}

check() {
    for hero in GondorPippin ArnorFattyBolger; do
        for idle_frame in 205 214 415; do
            expect_stealth_state "$idle_frame" "$hero" 0 0
        done
        for moving_frame in 231 245; do
            expect_stealth_state "$moving_frame" "$hero" 1 0
        done
    done

    expect_stealth_state 438 GondorPippin 0 1
    expect_stealth_state 438 ArnorFattyBolger 0 0
    expect_stealth_state 458 GondorPippin 1 0
    if covers 438; then
        expect 'TEST hobbit-stealth-source 438 GondorPippin ModuleTag_StealthForEscape type=1 enabled=1 eligible=1'
    fi
    if covers 458; then
        expect 'TEST hobbit-stealth-source 458 GondorPippin ModuleTag_StealthForEscape type=1 enabled=0 eligible=0'
    fi
}

compare() {
    for trace_phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 ~ /^hobbit-stealth/ && $3 >= first' \
            "$out/$trace_phase.log" > "$out/hobbit-stealth-$trace_phase.trace"
    done
    diff -u "$out/hobbit-stealth-continue.trace" "$out/hobbit-stealth-resume.trace"
}
