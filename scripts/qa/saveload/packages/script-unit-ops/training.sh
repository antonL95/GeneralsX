check() {
    if covers 246; then
        expect "TEST script-unit-ops-init frame=246 r0=1 r1=2 r2=3"
    fi
    for frame in 248 252 300; do
        if covers "$frame"; then
            taunting=1
            [ "$frame" -lt 300 ] || taunting=0
            expect "TEST script-unit-ops frame=$frame rank=3 taunting=$taunting goal=1"
        fi
    done
}
