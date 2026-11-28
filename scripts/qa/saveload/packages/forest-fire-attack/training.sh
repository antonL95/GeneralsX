check() {
    if covers 193; then
        expect 'TEST forest-fire attack frame=193 can=0'
    fi
    if covers 205; then
        expect 'TEST forest-fire health frame=205 name=ForestFireNeutralTree hp=1000'
    fi
    if covers 266; then
        expect 'TEST forest-fire attack frame=266 can=1'
    fi
    if covers 300; then
        hp=$(sed -n 's/^TEST forest-fire health frame=300 name=ForestFireNeutralTree hp=//p' "$log" | tail -n 1)
        if [ -z "$hp" ] || [ "$hp" -ge 1000 ]; then
            printf 'Expected ForestFireNeutralTree hp < 1000 at frame 300, got %s\n' "${hp:-missing}" >&2
            return 1
        fi
    fi
    if covers 332; then
        expect 'TEST forest-fire victim frame=332 name=ForestFireIdleArcher tree=0'
    fi
    if covers 362; then
        expect 'TEST forest-fire victim frame=362 name=ForestFireIdleArcher tree=0'
    fi
    if covers 395; then
        expect 'TEST forest-fire victim frame=395 name=ForestFireIdleArcher tree=0'
        expect 'TEST forest-fire health frame=395 name=ForestFireNeutralTree hp=1000'
    fi
}
