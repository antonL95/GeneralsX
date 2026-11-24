prepare() {
    if grep -q ' create Player_2 SkirmishMillPorter ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_2 SkirmishMillPorter ' "$out/continue.txt"; then
        return 0
    fi
    if covers 80; then
        expect 'TEST skirmish-mill frame=80 pending=0 grove=0 outside-base=0' || return 1
    fi
    for frame in 132 133; do
        if covers "$frame"; then
            expect "TEST skirmish-mill frame=$frame pending=1 grove=1 outside-base=1" || return 1
        fi
    done
    printf 'PASS targetless lumber mill keeps its grove construction task across reload (%s)\n' "$phase"
}
