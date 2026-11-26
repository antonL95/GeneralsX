check() {
    if covers 256; then
        if ! grep -Eq 'AUDIO TRACE frame=251 kind=sound-queue owner=[0-9]+ handle=[0-9]+ event=CastleMemberEvaBuildLoop' "$log"; then
            printf 'Missing CastleMemberEvaBuildLoop sound-queue trace in %s\n' "$phase" >&2
            return 1
        fi
        if ! grep -Eq 'AUDIO TRACE frame=252 kind=start owner=[0-9]+ handle=[0-9]+ event=CastleMemberEvaBuildLoop detail=attack' "$log"; then
            printf 'Missing CastleMemberEvaBuildLoop start trace in %s\n' "$phase" >&2
            return 1
        fi
        if ! grep -Eq 'AUDIO TRACE frame=256 kind=segment owner=[0-9]+ handle=[0-9]+ event=CastleMemberEvaBuildLoop detail=decay' "$log"; then
            printf 'Missing CastleMemberEvaBuildLoop decay stop trace in %s\n' "$phase" >&2
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
