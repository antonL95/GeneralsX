check() {
    if covers 205; then
        expect 'TEST script-cast-player frame=205 allowed=0 accepted=0' || return 1
    fi
    if covers 206; then
        expect 'TEST script-cast-ai-btn frame=206 accepted=0' || return 1
    fi
    if covers 207; then
        expect 'TEST script-cast-map frame=207 accepted=1' || return 1
    fi
    if covers 210; then
        expect 'TEST script-cast-count frame=210 markers=1' || return 1
    fi
    if covers 220; then
        expect 'TEST script-cast-disable frame=220 disabled=1' || return 1
    fi
    if covers 222; then
        expect 'TEST script-cast-player frame=222 allowed=0 accepted=0' || return 1
    fi
    if covers 223; then
        expect 'TEST script-cast-ai-btn frame=223 accepted=0' || return 1
    fi
    if covers 225; then
        expect 'TEST script-cast-map frame=225 accepted=1' || return 1
    fi
    if covers 228; then
        expect 'TEST script-cast-count frame=228 markers=2' || return 1
    fi
    if covers 248; then
        expect 'TEST script-cast-player frame=248 allowed=0 accepted=0' || return 1
    fi
    if covers 250; then
        expect 'TEST script-cast-obj frame=250 accepted=1' || return 1
    fi
    if covers 252; then
        expect 'TEST script-cast-script-btn frame=252 accepted=1' || return 1
    fi
    if covers 255; then
        expect 'TEST script-cast-count frame=255 markers=4' || return 1
    fi
    if covers 260; then
        expect 'TEST script-cast-disable frame=260 disabled=0' || return 1
    fi
}
