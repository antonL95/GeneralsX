prepare() {
    if grep -q ' create Player_1 SkirmishRaidFilterFarm ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_1 SkirmishRaidFilterFarm ' "$out/continue.txt"; then
        return 0
    fi
    for frame in 80 81; do
        if covers "$frame"; then
            expect "TEST deposit-xp-raid frame=$frame template=SkirmishRaidFilterHorde attacking=1" || return 1
            expect "TEST deposit-xp-raid frame=$frame template=SkirmishRaidFilterArcher attacking=0" || return 1
            expect "TEST deposit-xp-raid frame=$frame template=SkirmishRaidFilterHero attacking=0" || return 1
            expect "TEST deposit-xp-raid frame=$frame template=SkirmishRaidFilterDozer attacking=0" || return 1
            expect "TEST deposit-xp-raid frame=$frame template=SkirmishRaidFilterMachine attacking=0" || return 1
        fi
    done
    printf 'PASS only the plain squad raids the enemy farm (%s)\n' "$phase"
}
