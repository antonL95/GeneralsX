object_id() {
    sed -n "s/^TEST frame $1: create $2 for $3 id=\([0-9]*\) .*/\1/p" "$log"
}

check() {
    victim=$(sed -n 's/^TEST frame 220: create OwnAttackAlertVictim for Player_1 id=\([0-9]*\) .*/\1/p' "$out/save.log")
    friendly=$(sed -n 's/^TEST frame 220: create OwnAttackAlertFriendlyCaster for Player_1 id=\([0-9]*\) .*/\1/p' "$out/save.log")
    [ -n "$victim" ] && [ -n "$friendly" ] || return 1
    if covers 320; then
        grep -Eq '^TEST own-attack-probe frame=320 case=own health=' "$log" || return 1
        awk '/^TEST own-attack-probe frame=320 case=own / { split($5, a, "="); seen = 1; if (a[2] >= 5000 || a[2] <= 0) exit 1 } END { if (!seen) exit 1 }' "$log" || return 1
        grep -E 'weapon=OwnAttackAlertFriendlyFire .*victim=.*OwnAttackAlertVictim.*dealt=3\.00' "$log" || return 1
        count=$(grep -c "^TEST own-attack-event victim=$victim source=$friendly$" "$log" || true)
        printf 'Own sourced fire alert events: %s\n' "$count"
        [ "$count" -eq 0 ] || return 1
        count=$(sed -n '/TEST own-attack-cast frame=230/,/TEST own-attack-probe frame=320 case=own/p' "$log" | grep -c "^TEST own-attack-event victim=$victim source=0$" || true)
        printf 'Unattributed terrain fire alert events: %s\n' "$count"
        [ "$count" -gt 0 ] || return 1
    fi
    if covers 400; then
        enemy=$(object_id 330 OwnAttackAlertStatusEnemy PlyrCreeps)
        [ -n "$enemy" ] || return 1
        grep -Eq '^TEST own-attack-probe frame=400 case=status health=.* wet=1$' "$log" || return 1
        count=$(grep -c "^TEST own-attack-event victim=$victim source=$enemy$" "$log" || true)
        printf 'Enemy status alert events: %s\n' "$count"
        [ "$count" -eq 1 ] || return 1
    fi
    if covers 500; then
        enemy=$(object_id 415 OwnAttackAlertEnemy PlyrCreeps)
        [ -n "$enemy" ] || return 1
        grep -E 'weapon=OwnAttackAlertEnemyWeapon .*victim=.*OwnAttackAlertVictim.*dealt=20\.00' "$log" || return 1
        count=$(grep -c "^TEST own-attack-event victim=$victim source=$enemy$" "$log" || true)
        printf 'Enemy attack alert events: %s\n' "$count"
        [ "$count" -eq 1 ] || return 1
    fi
}
