check() {
    if covers 202; then
        expect 'TEST unpause-sp frame=202 power=UnpauseSpImmediatePower ready=0 paused=1 pct=0.000' || return 1
        expect 'TEST unpause-sp frame=202 power=UnpauseSpObeyPower ready=0 paused=1 pct=0.000' || return 1
    fi
    if covers 206; then
        expect 'TEST unpause-sp frame=206 power=UnpauseSpImmediatePower ready=1 paused=0 pct=1.000' || return 1
        expect 'TEST unpause-sp frame=206 power=UnpauseSpObeyPower ready=0 paused=0 pct=0.008' || return 1
    fi
    if covers 208; then
        expect 'TEST unpause-sp-cast frame=208 power=UnpauseSpImmediatePower allowed=1 accepted=1' || return 1
        expect 'TEST unpause-sp-cast frame=208 power=UnpauseSpObeyPower allowed=0 accepted=0' || return 1
    fi
    if covers 235; then
        expect 'TEST unpause-sp frame=235 power=UnpauseSpImmediatePower ready=0 paused=0 pct=0.225' || return 1
        expect 'TEST unpause-sp frame=235 power=UnpauseSpObeyPower ready=0 paused=0 pct=0.250' || return 1
    fi
    if covers 250; then
        expect 'TEST unpause-sp frame=250 power=UnpauseSpImmediatePower ready=0 paused=0 pct=0.350' || return 1
        expect 'TEST unpause-sp frame=250 power=UnpauseSpObeyPower ready=0 paused=0 pct=0.375' || return 1
    fi
    if covers 265; then
        expect 'TEST unpause-sp frame=265 power=UnpauseSpImmediatePower ready=0 paused=1 pct=0.433' || return 1
        expect 'TEST unpause-sp frame=265 power=UnpauseSpObeyPower ready=0 paused=1 pct=0.458' || return 1
        expect 'TEST unpause-sp-cast frame=265 power=UnpauseSpImmediatePower allowed=0 accepted=0' || return 1
    fi
    if covers 275; then
        expect 'TEST unpause-sp frame=275 power=UnpauseSpObeyPower ready=0 paused=1 pct=0.458' || return 1
    fi
    if covers 287; then
        expect 'TEST unpause-sp frame=287 power=UnpauseSpImmediatePower ready=1 paused=0 pct=1.000' || return 1
        expect 'TEST unpause-sp frame=287 power=UnpauseSpObeyPower ready=0 paused=0 pct=0.475' || return 1
    fi
    if covers 355; then
        expect 'TEST unpause-sp frame=355 power=UnpauseSpObeyPower ready=1 paused=0 pct=1.000' || return 1
    fi
    if covers 356; then
        expect 'TEST unpause-sp-cast frame=356 power=UnpauseSpObeyPower allowed=1 accepted=1' || return 1
    fi
}
