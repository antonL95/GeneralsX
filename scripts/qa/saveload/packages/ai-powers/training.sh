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
    expect_stance 575 AiPowersStanceInjured Battle 350.0
}
