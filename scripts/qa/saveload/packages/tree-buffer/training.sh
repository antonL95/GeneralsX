check() {
    if covers 15; then
        expect 'TEST tree-buffer state frame=15 fire-standing=1 fire-burning=0 crush-toppled=0 grab-near=1 grab-far=1 held=0 scale=0'
    fi
    if covers 25; then
        expect 'TEST tree-buffer grab-order frame=25 sent=1'
    fi
    if covers 30; then
        expect 'TEST tree-buffer origin frame=30 accepted=1 has-pos=1'
    fi
    if covers 200; then
        expect 'TEST tree-buffer state frame=200 fire-standing=1 fire-burning=0 crush-toppled=1 grab-near=0 grab-far=1 held=1 scale=1'
    fi
    for frame in 245 260; do
        if covers "$frame"; then
            expect "TEST tree-buffer state frame=$frame fire-standing=0 fire-burning=1 crush-toppled=1 grab-near=0 grab-far=1 held=1 scale=1"
        fi
    done
}
