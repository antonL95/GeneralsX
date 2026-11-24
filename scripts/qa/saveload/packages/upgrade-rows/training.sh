check() {
    if covers 238; then
        expect 'TEST pkg28 parent 238 UpgradeRowsTower live=1 dead=0 granted=0 obsolete=0 sold=0'
    fi
    for frame in 241 246 254; do
        if covers "$frame"; then
            anim=1
            if [ "$frame" = 254 ]; then
                anim=0
            fi
            expect "TEST pkg28 child $frame UpgradeRowsTower UpgradeRowsPayload dead=1 anim=$anim offset=12.0,0.0,0.0 angle=0.000 opacity=1.000 weapon=0 radius=3.0"
        fi
    done
    for frame in 250 251 253; do
        if covers "$frame"; then
            active=0
            if [ "$frame" = 251 ]; then
                active=1
            fi
            expect "TEST upgrade-rows subframe $frame user2=$active"
        fi
    done
    if covers 275; then
        expect 'TEST pkg28 parent 275 UpgradeRowsTower live=0 dead=0 granted=0 obsolete=0 sold=0'
    fi
}
