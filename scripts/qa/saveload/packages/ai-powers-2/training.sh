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
    if covers 243 && ! grep -F 'TEST ability 243 Player_2 AiPowers3FearCaster ' "$log" | awk '
        /cond=.*UNPACKING/ { pending++ }
        END { exit (pending != 1) }
    '; then
        printf 'Expected one fear caster to be unpacking at frame 243\n' >&2
        grep -F 'TEST ability 243 Player_2 AiPowers3FearCaster ' "$log" >&2 || true
        return 1
    fi
    if covers 385; then
        expect 'TEST powers-fire-state frame=385 template=AiPowers3FearCaster alive=2 ready=1' || return 1
        expect_heal_health 385 AiPowers3FearCaster 0 850.00 || return 1
        expect_heal_health 385 AiPowers3FearCaster 1 550.00 || return 1
    fi
    if covers 660; then
        expect 'TEST powers-fire-state frame=660 template=AiPowers3FreezeCaster alive=2 ready=1' || return 1
        expect 'TEST powers-fire-state frame=660 template=AiPowers3FreezeMarker alive=1' || return 1
        if ! grep -F 'TEST health 660 Player_2 AiPowers3FreezeMarker 0 ' "$log" | grep -Fq ' pos=2860.0,1410.0'; then
            printf 'Expected the freeze cast at the enemy sector sample\n' >&2
            grep -F 'TEST health 660 Player_2 AiPowers3FreezeMarker 0 ' "$log" >&2 || true
            return 1
        fi
    fi
    if covers 900; then
        expect 'TEST powers-fire-state frame=900 template=AiPowers3ReinforcementsCaster alive=1 ready=1' || return 1
    fi
    if covers 1065; then
        expect 'TEST powers-fire-state frame=1065 template=AiPowers3ReinforcementsCaster alive=1 ready=1' || return 1
    fi
    if covers 1270; then
        expect 'TEST powers-fire-state frame=1270 template=AiPowers3ReinforcementsCaster alive=1 ready=0' || return 1
        expect 'TEST powers-fire-state frame=1270 template=AiPowers3ReinforcementsMarker alive=1' || return 1
    fi
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
    if covers 243 && ! grep -F 'TEST ability 243 Player_2 AiPowers2WizardCaster ' "$log" | awk '
        /cond=.*UNPACKING/ { pending++ }
        END { exit (pending != 2) }
    '; then
        printf 'Expected wounded and crowded Wizard Blast casters to be unpacking at frame 243\n' >&2
        grep -F 'TEST ability 243 Player_2 AiPowers2WizardCaster ' "$log" >&2 || true
        return 1
    fi
    if covers 420; then
        expect 'TEST powers-fire-state frame=420 template=AiPowers2WizardCaster alive=3 ready=1' || return 1
        if ! awk '
            $1 == "TEST" && $2 == "impact" && $4 == "weapon=AiPowers2WizardWeapon" {
                if ($7 == "pos=2835.4,1415.4") wounded++
                else if ($7 == "pos=2835.4,1235.4") crowded++
                else unexpected++
            }
            END { exit (wounded != 1 || crowded != 1 || unexpected != 0) }
        ' "$log"; then
            printf 'Expected Wizard Blast impacts on the two sampled enemy clusters\n' >&2
            grep -F ' weapon=AiPowers2WizardWeapon ' "$log" >&2 || true
            return 1
        fi
    fi
    if covers 855; then
        expect 'TEST powers-fire-state frame=855 template=AiPowers2DeepCaster alive=2 ready=1' || return 1
        expect 'TEST powers-fire-state frame=855 template=AiPowers2DeepMarker alive=1' || return 1
        if ! grep -F 'TEST health 855 Player_2 AiPowers2DeepMarker 0 ' "$log" | grep -Fq ' pos=2849.5,1429.5'; then
            printf 'Expected Call of the Deep at the occupied diagonal sample\n' >&2
            grep -F 'TEST health 855 Player_2 AiPowers2DeepMarker 0 ' "$log" >&2 || true
            return 1
        fi
    fi
    if covers 1060; then
        expect 'TEST powers-fire-state frame=1060 template=AiPowers2BuffCaster alive=3 ready=1' || return 1
        expect 'TEST powers-fire-state frame=1060 template=AiPowers2BuffMarker alive=1' || return 1
        if ! grep -F 'TEST health 1060 Player_2 AiPowers2BuffMarker 0 ' "$log" | grep -Fq ' pos=2890.0,1320.0'; then
            printf 'Expected the area buff on the distant target with nearby allies\n' >&2
            grep -F 'TEST health 1060 Player_2 AiPowers2BuffMarker 0 ' "$log" >&2 || true
            return 1
        fi
    fi
    if covers 1320; then
        expect 'TEST powers-fire-state frame=1320 template=AiPowers2CitadelCaster alive=1 ready=0' || return 1
        expect 'TEST powers-fire-state frame=1320 template=AiPowers2CitadelControl alive=1 ready=1' || return 1
        expect 'TEST powers-fire-state frame=1320 template=AiPowers2CitadelMarker alive=1' || return 1
    fi
    if covers 1435; then
        expect 'TEST powers-fire-state frame=1435 template=AiPowers2DominateAreaCaster alive=2 ready=1' || return 1
        for subject in 'Player_2 AiPowers2DominateElite 0' 'Player_2 AiPowers2DominateTroll 0' \
            'Player_1 AiPowers2DominateWounded 0' 'Player_1 AiPowers2DominateWounded 1' \
            'Player_1 AiPowers2DominateFootman 0' 'Player_1 AiPowers2DominateFootman 1'; do
            if ! grep -F "TEST health 1435 $subject " "$log" | grep -Fq ' dead=0 '; then
                printf 'Expected living target %s under its expected owner at frame 1435\n' "$subject" >&2
                return 1
            fi
        done
    fi
    if covers 580; then
        expect 'TEST powers-fire-state frame=580 template=AiPowers2TrainCaster alive=2 ready=1' || return 1
        expect 'TEST powers-fire-state frame=580 template=AiPowers2FrenzyCaster alive=2 ready=1' || return 1
        expect 'TEST powers-fire-state frame=580 template=AiPowers2TrainMarker alive=1' || return 1
        expect 'TEST powers-fire-state frame=580 template=AiPowers2FrenzyMarker alive=1' || return 1
        if ! grep -F 'TEST health 580 Player_2 AiPowers2TrainMarker 0 ' "$log" | grep -Fq ' pos=2842.4,1362.4'; then
            printf 'Expected archer training on the allied archer sample\n' >&2
            return 1
        fi
        if ! grep -F 'TEST health 580 Player_2 AiPowers2FrenzyMarker 0 ' "$log" | grep -Fq ' pos=3202.4,1492.4'; then
            printf 'Expected battle frenzy on the allied sample during combat\n' >&2
            return 1
        fi
    fi
}
