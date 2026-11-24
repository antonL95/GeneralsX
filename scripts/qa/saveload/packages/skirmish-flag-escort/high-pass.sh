prepare() {
    if grep -q '^80 skirmish-tactic-guard ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q '^80 skirmish-tactic-guard ' "$out/continue.txt"; then
        return 0
    fi
    for frame in 80 81; do
        if covers "$frame"; then
            expect "TEST skirmish-guard frame=$frame index=0 guarding=0" || return 1
            expect "TEST skirmish-guard frame=$frame index=1 guarding=1" || return 1
        fi
    done
    if covers 620; then
        grep -Eq '^TEST neutralstate frame=620 object=CaptureFlag .* owner=4 .* capture=0 captured=1 ' "$log" &&
        grep -Eq '^TEST neutralstate frame=620 object=SignalFire .* owner=4 .* captured=1 ' "$log" &&
        [ "$(grep -Ec '^TEST skirmish-guard frame=620 index=[01] guarding=0$' "$log")" = 2 ] || {
            printf 'Missing flag ownership or escort release (%s)\n' "$phase"
            return 1
        }
        printf 'PASS targetless flag escort survives reload and releases after capture (%s)\n' "$phase"
    fi
}
