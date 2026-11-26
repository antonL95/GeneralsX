pt_expect() {
    if ! covers "$1"; then
        return 0
    fi
    expect "$2"
}

check() {
    pt_expect 244 'TEST pt-dismount-check 244 MorgulWitchKingOnFellBeast x=1540 y=450 shroud=0 canDo=1 validTarget=1' || return 1
    pt_expect 244 'TEST pt-dismount-check 244 MorgulWitchKingOnFellBeast x=1990 y=450 shroud=0 canDo=1 validTarget=1' || return 1
    pt_expect 270 'TEST pt-dismount-state 270 MorgulWitchKingOnFellBeast absent' || return 1
    pt_expect 270 'TEST pt-dismount-state 270 MorgulWitchKingOnFellBeastUpgrade present cmdset=MordorWitchKingFBUpgradeCommandSet' || return 1

    pt_expect 115 'TEST pt-grab 115 RohanEntOak toggle=1 valid=1 accepted=0' || return 1
    pt_expect 115 'TEST pt-grab 115 RohanEntFir toggle=1 valid=1 accepted=0' || return 1
    pt_expect 130 'TEST pt-grab 130 RohanEntOak toggle=0 valid=1 accepted=1' || return 1
    pt_expect 130 'TEST pt-grab 130 RohanEntFir toggle=0 valid=1 accepted=1' || return 1
    pt_expect 244 'TEST pt-held 244 RohanEntOak count=1' || return 1
    pt_expect 244 'TEST pt-held 244 RohanEntFir count=1' || return 1
    pt_expect 260 'TEST pt-held 260 RohanEntOak count=1' || return 1
    pt_expect 260 'TEST pt-held 260 RohanEntFir count=1' || return 1

    pt_expect 110 'TEST pt-troll-check 110 MordorMountainTroll count=0 ws1=0 modelAllowed=1 containerAllowed=1 canDo=1' || return 1
    pt_expect 120 'TEST pt-troll-check 120 MordorMountainTroll count=1 ws1=1 modelAllowed=0 containerAllowed=1 canDo=0' || return 1
    pt_expect 250 'TEST pt-troll-check 250 MordorMountainTroll count=1 ws1=1 modelAllowed=0 containerAllowed=1 canDo=0' || return 1

    pt_expect 270 'TEST pt-carrier-state 270 PowerTargetsCarrier count=2 ws1=1 ws2=1' || return 1
    pt_expect 280 'TEST pt-carrier-state 280 PowerTargetsCarrier count=1 ws1=1 ws2=0' || return 1
    pt_expect 290 'TEST pt-carrier-state 290 PowerTargetsCarrier count=0 ws1=0 ws2=0' || return 1
}

compare() {
    for trace_phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 ~ /^pt-/ && $3 >= first' \
            "$out/$trace_phase.log" > "$out/power-targets-$trace_phase.trace"
    done
    diff -u "$out/power-targets-continue.trace" "$out/power-targets-resume.trace"
}
