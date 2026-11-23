check() {
    for frame in 242 245 310; do
        if covers "$frame"; then
            timed=1
            [ "$frame" -lt 310 ] || timed=0
            expect "TEST skirmish-opcode frame=$frame counter=7 reference=1 distance=1 distant=0 missing=0 team=1 model=$timed"
        fi
    done
}
