check() {
    if covers 225; then
        expect "TEST powers-fire-state frame=225 template=MordorWitchKing alive=1 upgrade=1"
    fi
    if covers 226; then
        expect "TEST powers-fire-state frame=226 template=MordorWitchKing alive=1 ready=1"
    fi
    if covers 230; then
        expect "TEST powers-fire-cast frame=230 button=Command_HourOfTheWitchKingDualButton issued=1"
    fi
    if covers 232; then
        expect "TEST powers-fire-cast frame=232 button=Command_DeployMine issued=1"
    fi
    if covers 330; then
        expect "TEST powers-fire-state frame=330 template=MordorWitchKing alive=1 ready=0"
    fi
    if covers 400; then
        expect "TEST powers-fire-state frame=400 template=IsengardDeployedExplosiveMine alive=1"
        expect "TEST powers-fire-state frame=400 template=IsengardExplosiveMine alive=0"
    fi
}
