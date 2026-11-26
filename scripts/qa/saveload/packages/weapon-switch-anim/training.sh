check() {
    if covers 229; then
        expect "WEAPON-SWITCH-ANIM frame=229 actor=ElvenRivendellKnightHorde toggle1=0 swapping1=0 members=0/10 weapon=RivendellKnightSpear reloading=0 readyAfter=0" || return
        expect "WEAPON-SWITCH-ANIM frame=229 actor=GondorFaramir toggle1=0 swapping1=0 members=0/0 weapon=FaramirBow reloading=1 readyAfter=1" || return
    fi
    if covers 232; then
        expect "WEAPON-SWITCH-ANIM frame=232 actor=ElvenRivendellKnightHorde toggle1=1 swapping1=1 members=10/10 weapon=RivendellKnightBow reloading=1 readyAfter=1" || return
    fi
    if covers 233; then
        expect "WEAPON-SWITCH-ANIM frame=233 actor=GondorFaramir toggle1=1 swapping1=1 members=0/0 weapon=FaramirSword reloading=0 readyAfter=0" || return
    fi
    if covers 244; then
        expect "WEAPON-SWITCH-ANIM frame=244 actor=ElvenRivendellKnightHorde toggle1=1 swapping1=1 members=10/10 weapon=RivendellKnightBow reloading=1 readyAfter=1" || return
        expect "WEAPON-SWITCH-ANIM frame=244 actor=GondorFaramir toggle1=1 swapping1=1 members=0/0 weapon=FaramirSword reloading=0 readyAfter=0" || return
    fi
    if covers 261; then
        expect "WEAPON-SWITCH-ANIM frame=261 actor=ElvenRivendellKnightHorde toggle1=1 swapping1=1 members=10/10 weapon=RivendellKnightBow reloading=1 readyAfter=1" || return
    fi
    if covers 262; then
        expect "WEAPON-SWITCH-ANIM frame=262 actor=ElvenRivendellKnightHorde toggle1=1 swapping1=0 members=0/10 weapon=RivendellKnightBow reloading=1 readyAfter=1" || return
        expect "WEAPON-SWITCH-ANIM frame=262 actor=GondorFaramir toggle1=1 swapping1=1 members=0/0 weapon=FaramirSword reloading=0 readyAfter=0" || return
    fi
    if covers 263; then
        expect "WEAPON-SWITCH-ANIM frame=263 actor=GondorFaramir toggle1=1 swapping1=0 members=0/0 weapon=FaramirSword reloading=0 readyAfter=0" || return
    fi
    if covers 267; then
        expect "WEAPON-SWITCH-ANIM frame=267 actor=ElvenRivendellKnightHorde toggle1=0 swapping1=1 members=10/10 weapon=RivendellKnightSpear reloading=0 readyAfter=0" || return
    fi
    if covers 268; then
        expect "WEAPON-SWITCH-ANIM frame=268 actor=GondorFaramir toggle1=0 swapping1=1 members=0/0 weapon=FaramirBow reloading=1 readyAfter=1" || return
    fi
    if covers 296; then
        expect "WEAPON-SWITCH-ANIM frame=296 actor=ElvenRivendellKnightHorde toggle1=0 swapping1=1 members=10/10 weapon=RivendellKnightSpear reloading=0 readyAfter=0" || return
    fi
    if covers 297; then
        expect "WEAPON-SWITCH-ANIM frame=297 actor=ElvenRivendellKnightHorde toggle1=0 swapping1=0 members=0/10 weapon=RivendellKnightSpear reloading=0 readyAfter=0" || return
        expect "WEAPON-SWITCH-ANIM frame=297 actor=GondorFaramir toggle1=0 swapping1=1 members=0/0 weapon=FaramirBow reloading=1 readyAfter=1" || return
    fi
    if covers 298; then
        expect "WEAPON-SWITCH-ANIM frame=298 actor=GondorFaramir toggle1=0 swapping1=0 members=0/0 weapon=FaramirBow reloading=1 readyAfter=1" || return
    fi
}
