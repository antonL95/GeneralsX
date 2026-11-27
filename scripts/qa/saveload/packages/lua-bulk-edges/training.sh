check() {
    if covers 9; then expect 'TEST lua bulk condition frame=9 player=Player_1 set=0' || return 1; fi
    for frame in 220 245; do
        if covers "$frame"; then expect "TEST lua bulk condition frame=$frame player=Player_1 set=1" || return 1; fi
    done
    if covers 220; then
        grep -E '^TEST bcaravan object frame=220 template=GuldurSlaveMill .* x=3350 y=500 progress=[1-9][0-9]?$' "$log" || return 1
    fi
    if covers 900; then
        expect 'TEST bcaravan census frame=900 template=LuaBulkEdgePorter living=0' || return 1
        grep -E '^TEST bcaravan object frame=900 template=GuldurSlaveMill .* x=3350 y=500 progress=-1$' "$log" || return 1
    fi
    if covers 920; then
        expect 'TEST lua bulk condition frame=920 player=PlyrCreeps set=1' || return 1
        expect 'TEST check-upgrade frame=920 found=0' || return 1
    fi
    if covers 921; then expect 'TEST check-upgrade frame=921 found=1' || return 1; fi
}
