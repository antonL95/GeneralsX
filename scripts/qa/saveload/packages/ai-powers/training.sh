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

check() {
    expect_stance 243 AiPowersStanceHero Battle 1000.0 || return 1
    expect_stance 305 AiPowersStanceHero Battle 1000.0 || return 1
    expect_stance 480 AiPowersStanceHero Aggressive 1000.0 || return 1
    expect_stance 480 AiPowersStanceInjured Aggressive 1000.0 || return 1
    expect_stance 480 AiPowersStanceHoldControl Battle 1000.0 || return 1
    expect_stance 575 AiPowersStanceInjured Battle 350.0
}
