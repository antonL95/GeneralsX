prepare() {
    if grep -q ' create Player_2 StrategicBiasArcher ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' create Player_2 StrategicBiasArcher ' "$out/continue.txt"; then
        return 0
    fi
    if covers 150; then
        expect "TEST ai-target frame=150 source=StrategicBiasArcher skirmishAI=1 target=StrategicBiasFar targetOwner=Player_1" || return 1
        expect "TEST ai-target frame=150 source=StrategicBiasArcher skirmishAI=0 target=StrategicBiasNear targetOwner=PlyrCreeps" || return 1
    fi
    printf 'PASS skirmish AI prefers its strategic enemy within the 100 unit bias across reload (%s)\n' "$phase"
}
