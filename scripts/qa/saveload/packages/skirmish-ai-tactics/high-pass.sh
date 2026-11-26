prepare() {
    if grep -q ' create Player_2 SkirmishTacticsPorter ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_2 SkirmishTacticsPorter ' "$out/continue.txt"; then
        return 0
    fi
    if covers 80; then
        expect 'TEST skirmish-mill frame=80 pending=0 grove=0 outside-base=0' || return 1
    fi
    for frame in 200 550; do
        if covers "$frame"; then
            expect "TEST skirmish-mill frame=$frame pending=1 grove=1 outside-base=1" || return 1
        fi
    done
    if covers 1090; then
        expect 'TEST skirmish-mill frame=1090 pending=0 grove=0 outside-base=0' || return 1
    fi
    printf 'PASS lumber mill marker veto, farm fallback and 30s timeout match across reload (%s)\n' "$phase"
}
