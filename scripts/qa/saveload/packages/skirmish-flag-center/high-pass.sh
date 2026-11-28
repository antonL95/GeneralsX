prepare() {
    if grep -q ' create PlyrCivilian SkirmishFlagCenterTargetFlag ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
        printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
    fi
}

check() {
    if ! grep -q ' create PlyrCivilian SkirmishFlagCenterTargetFlag ' "$out/continue.txt"; then
        return 0
    fi
    for frame in 200 201; do
        if covers "$frame"; then
            expect "TEST skirmish-capture frame=$frame index=0 active=1 target=1" || return 1
            expect "TEST skirmish-guard frame=$frame index=1 guarding=1" || return 1
        fi
    done
    printf 'PASS flag squad chooses closest flag to squad center (%s)\n' "$phase"
}
