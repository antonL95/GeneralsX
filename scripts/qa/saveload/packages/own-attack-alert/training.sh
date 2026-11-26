check() {
    victim=$(sed -n 's/^TEST frame 220: create OwnAttackAlertVictim for Player_1 id=\([0-9]*\) .*/\1/p' "$out/save.log")
    [ -n "$victim" ] || return 1
    if covers 320; then
        grep -Eq '^TEST own-attack-probe frame=320 case=own health=' "$log" || return 1
        awk '/^TEST own-attack-probe frame=320 case=own / { split($5, a, "="); seen = 1; if (a[2] >= 5000 || a[2] <= 0) exit 1 } END { if (!seen) exit 1 }' "$log" || return 1
        count=$(sed -n '/TEST own-attack-cast frame=230/,/TEST own-attack-probe frame=320 case=own/p' "$log" | grep -c "^TEST own-attack-event victim=$victim$" || true)
        printf 'Own fire alert events: %s\n' "$count"
        [ "$count" -eq 0 ] || return 1
    fi
    if covers 400; then
        grep -Eq '^TEST own-attack-probe frame=400 case=status health=.* wet=1$' "$log" || return 1
        count=$(sed -n '/TEST frame 330: create OwnAttackAlertStatusEnemy/,/TEST own-attack-probe frame=400 case=status/p' "$log" | grep -c "^TEST own-attack-event victim=$victim$" || true)
        printf 'Enemy status alert events: %s\n' "$count"
        [ "$count" -eq 0 ] || return 1
    fi
    if covers 500; then
        grep -Eq '^TEST own-attack-probe frame=500 case=enemy health=' "$log" || return 1
        count=$(sed -n '/TEST frame 415: create OwnAttackAlertEnemy/,/TEST own-attack-probe frame=500 case=enemy/p' "$log" | grep -c "^TEST own-attack-event victim=$victim$" || true)
        printf 'Enemy attack alert events: %s\n' "$count"
        [ "$count" -eq 1 ] || return 1
    fi
}
