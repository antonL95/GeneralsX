check() {
    if covers 255; then
        if ! grep -Eq 'AUDIO TRACE frame=251 kind=sound-queue owner=[0-9]+ handle=[0-9]+ event=BuildingBigConstructionLoop' "$log"; then
            printf 'Missing BuildingBigConstructionLoop sound-queue trace in %s\n' "$phase" >&2
            return 1
        fi
        if ! grep -Eq 'AUDIO TRACE frame=25[56] kind=stop owner=[0-9]+ handle=[0-9]+ event=BuildingBigConstructionLoop' "$log"; then
            printf 'Missing BuildingBigConstructionLoop stop trace in %s\n' "$phase" >&2
            return 1
        fi
    fi
    if covers 264; then
        expect 'TEST castle-member-eva eva frame=264 event=CastleMemberEvaKeepAttack played=1'
    fi
    if covers 274; then
        expect 'TEST castle-member-eva eva frame=274 event=CastleBreached played=0'
    fi
    if covers 284; then
        expect 'TEST castle-member-eva eva frame=284 event=CastleBreached played=1'
    fi
    if covers 305; then
        expect 'TEST castle-member-eva eva frame=305 event=CampDestroyed played=1'
    fi
}
