expect_stance() {
    observation_frame=$1
    actor=$2
    stance=$3
    health=$4
    if covers "$observation_frame" && ! grep -F "TEST mods $observation_frame object " "$log" | grep -Fq "($actor) stance=$stance health=$health/1000.0 "; then
        printf '%s should be in %s at frame %s with health %s\n' "$actor" "$stance" "$observation_frame" "$health" >&2
        grep -F "($actor) stance=" "$log" >&2 || true
        return 1
    fi
}

expect_health() {
    observation_frame=$1
    actor=$2
    ordinal=$3
    health=$4
    if covers "$observation_frame" && ! grep -F "TEST health $observation_frame Player_1 $actor $ordinal " "$log" | grep -Fq "health=$health max=1000.00 "; then
        printf '%s %s should have health %s at frame %s\n' "$actor" "$ordinal" "$health" "$observation_frame" >&2
        grep -F "TEST health $observation_frame Player_1 $actor $ordinal " "$log" >&2 || true
        return 1
    fi
}

expect_pending() {
    actor=$1
    if covers 243 && ! grep -F "TEST ability 243 Player_2 $actor " "$log" | grep -Eq ' cond=([^,]+,)*UNPACKING(,|$)'; then
        printf '%s should be unpacking at frame 243\n' "$actor" >&2
        grep -F "TEST ability 243 Player_2 $actor " "$log" >&2 || true
        return 1
    fi
}

expect_combat_impacts() {
    if ! awk '
        $1 == "TEST" && $2 == "impact" && $4 == "weapon=AiPowersSpecialWeapon" {
            if ($7 == "pos=3128.0,1480.0") charge++
            else if ($7 == "pos=3440.0,1475.0") morgul++
            else unexpected++
        }
        END { exit (charge != 1 || morgul != 1 || unexpected != 0) }
    ' "$log"; then
        printf 'Expected one AI power impact on the nearby infantry and one on the eligible hero\n' >&2
        grep -F ' weapon=AiPowersSpecialWeapon ' "$log" >&2 || true
        return 1
    fi
}

expect_weapon() {
    observation_frame=$1
    actor=$2
    ordinal=$3
    weapon=$4
    if covers "$observation_frame" && ! grep -F "TEST ability $observation_frame Player_2 $actor " "$log" | sed -n "${ordinal}p" | grep -Fq " weapon=$weapon "; then
        printf '%s should have weapon %s at frame %s\n' "$actor" "$weapon" "$observation_frame" >&2
        grep -F "TEST ability $observation_frame Player_2 $actor " "$log" >&2 || true
        return 1
    fi
}

expect_mounted() {
    observation_frame=$1
    ordinal=$2
    mounted=$3
    if covers "$observation_frame" && ! grep -F "TEST ability $observation_frame Player_2 AiPowersMountCaster " "$log" | sed -n "${ordinal}p" | grep -Fq " mounted=$mounted "; then
        printf 'Expected mount state %s for AiPowersMountCaster %s at frame %s\n' "$mounted" "$ordinal" "$observation_frame" >&2
        grep -F "TEST ability $observation_frame Player_2 AiPowersMountCaster " "$log" >&2 || true
        return 1
    fi
}

check() {
    expect_pending AiPowersChargeCaster || return 1
    expect_pending AiPowersMorgulCaster || return 1
    expect_health 243 AiPowersHeroTarget 0 600.00 || return 1
    expect_health 400 AiPowersHeroTarget 0 550.00 || return 1
    if covers 400; then
        expect_combat_impacts || return 1
        expect 'TEST powers-fire-state frame=400 template=AiPowersChargeCaster alive=2 ready=1' || return 1
    fi
    expect_stance 243 AiPowersStanceHero Battle 1000.0 || return 1
    expect_stance 305 AiPowersStanceHero Battle 1000.0 || return 1
    expect_stance 480 AiPowersStanceHero Aggressive 1000.0 || return 1
    expect_stance 480 AiPowersStanceInjured Aggressive 1000.0 || return 1
    expect_stance 480 AiPowersStanceHoldControl Battle 1000.0 || return 1
    expect_stance 575 AiPowersStanceInjured Battle 350.0 || return 1
    if covers 745; then
        expect 'TEST powers-fire-state frame=745 template=AiPowersSummonMarker alive=0' || return 1
    fi
    if covers 875; then
        expect 'TEST powers-fire-state frame=875 template=AiPowersSummonMarker alive=1' || return 1
        expect 'TEST powers-fire-state frame=875 template=AiPowersSummonCaster alive=2 ready=1' || return 1
        if ! grep -F 'TEST health 875 Player_2 AiPowersSummonMarker 0 ' "$log" | grep -Fq ' pos=3432.4,1407.6'; then
            printf 'Expected the summon in the southeast cluster at frame 875\n' >&2
            grep -F 'TEST health 875 Player_2 AiPowersSummonMarker 0 ' "$log" >&2 || true
            return 1
        fi
    fi
    expect_weapon 965 AiPowersWeaponToggle 1 AiPowersToggleMeleeWeapon || return 1
    expect_weapon 965 AiPowersWeaponToggle 2 AiPowersToggleRangedWeapon || return 1
    expect_weapon 965 AiPowersSiegeToggle 1 AiPowersToggleRangedWeapon || return 1
    expect_weapon 1195 AiPowersWeaponToggle 1 AiPowersToggleRangedWeapon || return 1
    expect_weapon 1195 AiPowersSiegeToggle 1 AiPowersToggleMeleeWeapon || return 1
    expect_mounted 965 1 1 || return 1
    expect_mounted 965 2 0 || return 1
    expect_mounted 1195 1 0 || return 1
    expect_weapon 1075 AiPowersPoweredWeaponToggle 1 AiPowersToggleMeleeWeapon || return 1
    if covers 1035 && ! grep -Eq '^TEST formation-state 1035 AiPowersFormationToggle 0 .* active=1$' "$log"; then
        printf 'Expected the AI horde to enter its alternate formation at frame 1035\n' >&2
        grep -F 'TEST formation-state 1035 AiPowersFormationToggle ' "$log" >&2 || true
        return 1
    fi
    if covers 1440 && ! awk '
        $1 == "TEST" && $2 == "health" && $4 == "Player_2" && $5 == "AiPowersFollowMarker" {
            split($11, value, "=")
            split(value[2], pos, ",")
            dx = pos[1] - ($3 == 1315 ? 3430 : 3470)
            dy = pos[2] - ($3 == 1315 ? 1450 : 1470)
            if (dx * dx + dy * dy <= 100) near[$3] = 1
        }
        END { exit (!near[1315] || !near[1440]) }
    ' "$log"; then
        printf 'Expected the summoned unit to move toward each of the caster’s attack targets\n' >&2
        grep -F 'AiPowersFollowMarker 0 ' "$log" >&2 || true
        return 1
    fi
}
