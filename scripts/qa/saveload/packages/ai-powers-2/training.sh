expect_heal_health() {
    observation_frame=$1
    actor=$2
    ordinal=$3
    health=$4
    if covers "$observation_frame" && ! grep -F "TEST health $observation_frame Player_2 $actor $ordinal " "$log" | grep -Fq "health=$health max=1000.00 "; then
        printf '%s %s should have health %s at frame %s\n' "$actor" "$ordinal" "$health" "$observation_frame" >&2
        grep -F "TEST health $observation_frame Player_2 $actor " "$log" >&2 || true
        return 1
    fi
}

check() {
    if covers 243 && ! grep -F 'TEST ability 243 Player_2 AiPowers2Healer ' "$log" | awk '
        /cond=.*UNPACKING/ { pending++ }
        END { exit (pending != 2) }
    '; then
        printf 'Expected both healers to be unpacking and the control to be idle at frame 243\n' >&2
        grep -F 'TEST ability 243 Player_2 AiPowers2Healer ' "$log" >&2 || true
        return 1
    fi
    expect_heal_health 243 AiPowers2Healer 0 350.00 || return 1
    expect_heal_health 243 AiPowers2Hero 0 350.00 || return 1
    expect_heal_health 370 AiPowers2Healer 0 650.00 || return 1
    expect_heal_health 370 AiPowers2Hero 0 650.00 || return 1
    expect_heal_health 370 AiPowers2Hero 1 350.00 || return 1
    expect_heal_health 370 AiPowers2Infantry 0 350.00 || return 1
    if covers 370; then
        expect 'TEST powers-fire-state frame=370 template=AiPowers2Healer alive=3 ready=1' || return 1
    fi
}
