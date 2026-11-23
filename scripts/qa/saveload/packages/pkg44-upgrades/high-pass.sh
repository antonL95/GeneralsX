check() {
    if covers 75; then
        expect 'PKG44_POWER 75 Pkg44CapturePower 1' || return 1
    fi
    if covers 137; then
        expect 'PKG44_CAPTURE 137 Player_2 Pkg44Creep 1' || return 1
        expect 'PKG44_POWER 137 Pkg44CapturePower 0' || return 1
        expect 'PKG44_CAPTURE 137 PlyrCreeps Pkg44FarCreep 1' || return 1
    fi
    if covers 230; then
        expect "PKG44_UPGRADE 230 Pkg44UnitArmor 1" || return 1
        expect "PKG44_UPGRADE 230 Pkg44ImportantResearch 1" || return 1
        expect "PKG44_UPGRADE 230 Pkg44NormalResearch 0" || return 1
    fi
    if covers 295; then
        expect "PKG44_UPGRADE 295 Pkg44NormalResearch 1" || return 1
    fi
}
