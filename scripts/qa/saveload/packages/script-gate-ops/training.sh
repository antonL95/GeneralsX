check() {
    if covers 238; then
        expect "TEST script-gate-ops frame=238 needs=2 wantsOpen=1 passable=0 ready=0"
    fi
    if covers 285; then
        expect "TEST script-gate-ops frame=285 needs=2 wantsOpen=1 passable=1 ready=1"
    fi
    if covers 288; then
        expect "TEST script-gate-ops frame=288 needs=1 wantsOpen=1 passable=1 ready=1"
    fi
    if covers 340; then
        expect "TEST script-gate-ops frame=340 needs=-1 wantsOpen=0 passable=0 ready=1"
    fi
    if covers 343; then
        expect "TEST script-gate-ops-seq frame=343 needs=1 wantsOpen=1 passable=0 ready=0 seqDone=0"
    fi
    if covers 395; then
        expect "TEST script-gate-ops-seq frame=395 needs=-1 wantsOpen=0 passable=1 ready=1 seqDone=1"
    fi
}
