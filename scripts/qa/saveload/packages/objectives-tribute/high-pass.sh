fixture_active() {
    grep -q ' hud-tribute-setup ' "$out/commands.txt"
}

check() {
    fixture_active || return 0
    if covers 7; then expect "TEST hud-tribute 7 800 40"; fi
    if covers 8; then expect "TEST hud-tribute 8 800 40"; fi
    if covers 8; then
        expect "TEST hud-objective 8 1 1 0 1"
        expect "TEST hud-objective 8 100 0 0 1"
    fi
    if covers 9; then
        expect "TEST hud-objective 9 1 1 1 1"
        expect "TEST hud-objective 9 2 1 0 1"
    fi
    if covers 10; then expect "TEST hud-tribute-panel 10 1"; fi
    if covers 13; then expect "TEST hud-tribute 13 800 40"; fi
    if covers 14; then expect "TEST hud-tribute-panel 14 1"; fi
    if covers 18; then expect "TEST hud-tribute 18 0 840"; fi
    if covers 19; then
        expect "TEST hud-objective-panel 19 1 1"
        expect "TEST hud-objective 19 1 1 1 0"
    fi
    if covers 21; then
        expect "TEST hud-objective 21 1 1 1 0"
        expect "TEST hud-objective 21 2 1 0 0"
        expect "TEST hud-tribute 21 0 840"
    fi
    if covers 22; then expect "TEST hud-objective 22 2 0 0 1"; fi
    printf 'PASS objectives and tribute HUD state (%s)\n' "$phase"
}
