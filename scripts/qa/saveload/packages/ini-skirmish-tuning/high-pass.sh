enabled() {
    [ "$save_frame" -eq 80 ] && [ "$end" -eq 200 ] &&
        grep -Fqs 'MakeAllSkirmishSidesAIControlled = Yes' "$out/data/GeneralsX/GeneralsZH/Maps/map mp high pass/map.ini"
}

prepare() {
    if enabled; then
        printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
        printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
    fi
}

check() {
    if ! enabled; then
        return 0
    fi
    if ! grep -Eq '^AI_PHASE[[:space:]]+123[[:space:]]+3[[:space:]]+1[[:space:]]' "$log"; then
        printf 'Missing local human slot planner phase transition (%s)\n' "$phase"
        return 1
    fi
    printf 'PASS configured skirmish planner updates the local human slot (%s)\n' "$phase"
}
