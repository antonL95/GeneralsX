check() {
    if covers 246; then
        expect "TEST script-fog-reveal frame=246 before=2 afterReveal=1 afterFogOff=0 fog=0"
    fi
    if covers 250; then
        expect "TEST script-fog-reveal-probe frame=250 current=0 fogBefore=0 afterFogOn=0 fogAfter=0"
    fi
    if covers 256; then
        expect "TEST script-fog-reveal-probe frame=256 current=0 fogBefore=0 afterFogOn=1 fogAfter=1"
    fi
}
