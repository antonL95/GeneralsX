prepare() {
    if grep -q ' create Player_2 SkirmishRoamHorde ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI 1' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_2 SkirmishRoamHorde ' "$out/continue.txt"; then
        return 0
    fi
    if covers 124; then
        expect 'TEST skirmish-roam frame=124 advancing=0' || return 1
    fi
    for frame in 200 201; do
        if covers "$frame"; then
            expect "TEST skirmish-roam frame=$frame advancing=1" || return 1
        fi
    done
    printf 'PASS Easy roaming defense advances to an owned farm after its timer (%s)\n' "$phase"
}
