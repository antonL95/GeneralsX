check() {
    if covers 235; then
        expect "TEST cirdan-state frame=235 alive=1 roster=1 ignoresSelectAll=1"
    fi
    if covers 236; then
        expect "TEST cirdan-select-all-heroes frame=236 ran=1"
    fi
    if covers 237; then
        expect "TEST cirdan-selection frame=237 cirdan=0 control=1"
    fi
    if covers 239; then
        expect "TEST cirdan-selection frame=239 cirdan=0 control=1"
    fi
    if covers 240; then
        expect "TEST cirdan-roster-select frame=240 ran=1"
    fi
    if covers 241; then
        expect "TEST cirdan-selection frame=241 cirdan=1 control=0"
    fi
    if covers 246; then
        expect "TEST cirdan-state frame=246 alive=1 roster=1 ignoresSelectAll=1"
    fi
    if covers 247; then
        expect "TEST cirdan-select-all-heroes frame=247 ran=1"
    fi
    if covers 248; then
        expect "TEST cirdan-selection frame=248 cirdan=0 control=1"
    fi
    if covers 249; then
        expect "TEST cirdan-roster-select frame=249 ran=1"
    fi
    if covers 250; then
        expect "TEST cirdan-selection frame=250 cirdan=1 control=0"
    fi
}
