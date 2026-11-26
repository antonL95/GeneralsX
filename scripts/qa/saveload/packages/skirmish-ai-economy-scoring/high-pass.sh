fixture_enabled() {
    grep -Fqx '932 cash Player_2 - 0 6000' "$out/commands.txt"
}

prepare() {
    fixture_enabled || return 0
    printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' \
        'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
    printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
}

check() {
    fixture_enabled || return 0
    if covers 4; then
        awk -F '\t' '$1 == "AI_BUILD" && $2 == 4 && $5 == "MordorSlaughterHouse_ForAI" && $7 == 1 {
            print
            opened = 1
        } END { exit !opened }' "$log" || return 1
    fi
    if covers 1203; then
        awk '$1 == "TEST" && $2 == "heroes" && $3 == 1203 && $4 == "Player_2" {
            split($5, balance, "=")
            if (balance[1] == "money" && balance[2] >= 300 && balance[2] < 1000000) {
                print
                affordable = 1
            }
        } END { exit !affordable }' "$log" || return 1
    fi
    if covers 1204; then
        awk -F '\t' '$1 == "AI_BUILD" && $2 == 1204 && $5 == "MordorSlaughterHouse_ForAI" && $7 == 2 {
            print
            expanded = 1
        } END { exit !expanded }' "$log" || return 1
    fi
}
