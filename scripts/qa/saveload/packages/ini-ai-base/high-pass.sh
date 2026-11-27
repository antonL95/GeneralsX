prepare() {
    if grep -q ' ai-base-site Player_2 ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q ' ai-base-site Player_2 ' "$out/continue.txt" || ! covers 126; then
        return 0
    fi
    if ! grep -Eq '^TEST ai-base frame=126 (offset=256\.03,-22\.67 angle=1\.2727|offset=81\.39,227\.59 angle=2\.5311)$' "$log"; then
        printf 'No authored Orc Pit placement in %s\n' "$phase"
        return 1
    fi
    printf 'PASS skirmish dozer uses authored base position and facing (%s)\n' "$phase"
}
