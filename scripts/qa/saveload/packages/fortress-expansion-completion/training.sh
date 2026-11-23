check() {
    if covers 244; then
        grep -E '^TEST fortress-expansion 244 pad=1 id=[1-9][0-9]* percent=[1-9][0-9]?\.[0-9]{2} under=1 dead=0$' "$log" ||
            { printf '%s\n' "Expected a live plot expansion with construction progress at frame 244 ($phase)."; return 1; }
    fi
    if covers 1000; then
        grep -E '^TEST fortress-expansion 1000 pad=1 id=[1-9][0-9]* percent=-1\.00 under=0 dead=0$' "$log" ||
            { printf '%s\n' "Expected a live, completed plot expansion at frame 1000 ($phase)."; return 1; }
    fi
    if covers 1018; then
        grep -E '^TEST frame 1018: object [0-9]+ FortressFixtureBlockedFarm .* dead 0 construction 0\.00$' "$log" ||
            { printf '%s\n' "Expected a reserved site before the blocker appeared ($phase)."; return 1; }
    fi
    if covers 1250; then
        expect 'TEST frame 1250: no FortressFixtureBlockedFarm 0 of Player_1'
        grep -E '^TEST frame 1250: object [0-9]+ FortressFixtureBlocker .* dead 0 ' "$log" ||
            { printf '%s\n' "Expected the late blocker to remain alive ($phase)."; return 1; }
    fi
    if covers 1400; then
        grep -E '^TEST frame 1400: object [0-9]+ FortressFixtureFarm .* dead 0 construction -1\.00$' "$log" ||
            { printf '%s\n' "Expected the unobstructed free-standing building to complete at frame 1400 ($phase)."; return 1; }
    fi
}
