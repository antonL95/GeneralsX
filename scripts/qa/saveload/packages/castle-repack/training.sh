check() {
    if covers 229; then
        expect 'TEST castle-repack CastleRepackFortress frame=229 unpacked=1 keep=1 occupied=1 defensive=0 masked=1'
    fi
    if covers 243; then
        expect 'TEST castle-repack CastleRepackEmpty frame=243 unpacked=1 keep=0 occupied=0 defensive=0 masked=1'
        expect 'TEST castle-repack CastleRepackCamp frame=243 unpacked=1 keep=0 occupied=2 defensive=1 masked=1'
        expect 'TEST castle-repack CastleRepackFortress frame=243 unpacked=1 keep=0 occupied=1 defensive=0 masked=1'
    fi
    if covers 305; then
        expect 'TEST castle-repack CastleRepackEmpty frame=305 unpacked=0 keep=0 occupied=0 defensive=0 masked=0'
        expect 'TEST castle-repack CastleRepackCamp frame=305 unpacked=1 keep=0 occupied=2 defensive=1 masked=1'
        expect 'TEST castle-repack CastleRepackFortress frame=305 unpacked=1 keep=0 occupied=1 defensive=0 masked=1'
    fi
    if covers 309; then
        expect 'TEST castle-repack CastleRepackCamp frame=309 unpacked=1 keep=0 occupied=1 defensive=1 masked=1'
    fi
    for castle in CastleRepackCamp CastleRepackFortress; do
        if covers 340; then
            expect "TEST castle-repack $castle frame=340 unpacked=1 keep=0 occupied=0 defensive=0 masked=1"
        fi
        if covers 390; then
            expect "TEST castle-repack $castle frame=390 unpacked=0 keep=0 occupied=0 defensive=0 masked=0"
        fi
    done
}
