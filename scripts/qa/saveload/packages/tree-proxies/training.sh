check() {
    if covers 18; then
        expect 'TEST tree-proxies frame=18 held=- tree=0 standing=1 same=0'
    fi
    if covers 220; then
        expect 'TEST tree-proxies direct order frame=220 sent=1'
    fi
    for frame in 230 245 254; do
        if covers "$frame"; then
            expect "TEST tree-proxies direct frame=$frame tree=0 standing=1 scale=0"
        fi
    done
    if covers 400; then
        expect 'TEST tree-proxies direct frame=400 tree=1 standing=0 scale=1'
    fi
    for frame in 200 245 254; do
        if covers "$frame"; then
            grep -E "TEST tree-proxies frame=$frame held=[^ -]+ tree=1 standing=0 same=1" "$log" || return 1
        fi
    done
}
