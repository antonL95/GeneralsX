prepare() {
    if grep -q ' create Player_1 SkirmishPerimeterOpenFarm ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_1 SkirmishPerimeterOpenFarm ' "$out/continue.txt"; then
        return 0
    fi
    for frame in 80 81; do
        if covers "$frame"; then
            expect "TEST skirmish-raid frame=$frame index=0 attacking=1" || return 1
            expect "TEST skirmish-raid frame=$frame index=1 attacking=1" || return 1
        fi
    done
    printf 'PASS raid skips blocked perimeter farm and attacks open farm (%s)\n' "$phase"
}
