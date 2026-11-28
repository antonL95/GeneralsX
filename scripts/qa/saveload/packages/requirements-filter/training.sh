check() {
    if covers 202; then
        expect 'TEST req-filter frame=202 ready=1 paused=0 met=1 store=1 pct=1.000' || return 1
    fi
    if covers 205; then
        expect 'TEST req-filter-cast frame=205 allowed=1 accepted=1' || return 1
    fi
    if covers 220; then
        expect 'TEST req-filter frame=220 ready=0 paused=0 met=1 store=1 pct=0.125' || return 1
    fi
    if covers 227; then
        expect 'TEST req-filter frame=227 ready=0 paused=1 met=0 store=0 pct=0.167' || return 1
    fi
    if covers 240; then
        expect 'TEST req-filter frame=240 ready=0 paused=1 met=0 store=0 pct=0.167' || return 1
    fi
    if covers 241; then
        expect 'TEST req-filter-cast frame=241 allowed=0 accepted=0' || return 1
    fi
    if covers 248; then
        expect 'TEST req-filter frame=248 ready=0 paused=1 met=0 store=0 pct=0.167' || return 1
    fi
    if covers 257; then
        expect 'TEST req-filter frame=257 ready=0 paused=1 met=0 store=0 pct=0.167' || return 1
    fi
    if covers 267; then
        expect 'TEST req-filter frame=267 ready=0 paused=0 met=1 store=1 pct=0.183' || return 1
    fi
    if covers 360; then
        expect 'TEST req-filter frame=360 ready=0 paused=0 met=1 store=1 pct=0.958' || return 1
    fi
    if covers 385; then
        expect 'TEST req-filter frame=385 ready=1 paused=0 met=1 store=1 pct=1.000' || return 1
    fi
    if covers 388; then
        expect 'TEST req-filter-cast frame=388 allowed=1 accepted=1' || return 1
    fi
}
