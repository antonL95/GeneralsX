check() {
    for frame in 242 246 300; do
        if covers "$frame"; then
            active=1
            [ "$frame" -lt 300 ] || active=0
            expect "TEST team-script-ops emotion frame=$frame active=$active"
        fi
    done
    for frame in 242 246; do
        if covers "$frame"; then
            expect "TEST team-script-ops path frame=$frame first=1 reachable=1"
        fi
    done
    if covers 258; then
        expect 'TEST team-script-ops guard frame=258 reachable=1 missing=0 origin=1 fallback=1'
    fi
    if covers 242; then
        expect 'TEST team-script-ops score frame=242 win=0 loss=0 winPosition=0 lossPosition=0'
    fi
    if covers 254; then
        expect 'TEST team-script-ops score frame=254 win=1 loss=1 winPosition=1 lossPosition=1'
    fi
}
