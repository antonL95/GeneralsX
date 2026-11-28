check() {
    for frame in 242 246; do
        if covers "$frame"; then
            expect "TEST script-player-set-sum frame=$frame objEq3=1 bld=1 distFar=1 upgSingle=1 dist3d=1 base=1"
        fi
    done
    if covers 247; then
        expect "TEST script-player-set-sum-area frame=247 typeLt1=0 typeEq1=1 builtLt1=0 upgAreaLt1=0 kindLt1=0 objGt3=1 bld=1 distFar=1 upgSingle=1 dist3d=1 base=1"
    fi
}
