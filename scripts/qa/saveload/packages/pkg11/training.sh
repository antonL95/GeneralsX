expect_re() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if covers 1009; then
        expect_re '^TEST pkg11 state frame=1009 name=Pkg11Horde .*memberhp=1725.0$'
        expect_re '^TEST pkg11 state frame=1009 name=Pkg11HordeControl .*memberhp=1785.0$'
        expect 'TEST pkg11 pulse frame=1009 name=Pkg11Horde combat=1 delay=30'
        expect 'TEST pkg11 pulse frame=1009 name=Pkg11HordeControl combat=1 delay=60'
    fi
    if covers 1012; then
        expect_re '^TEST pkg11 state frame=1012 name=Pkg11Horde .*items=15 .*memberhp=1785.0$'
        expect_re '^TEST pkg11 state frame=1012 name=Pkg11HordeNoContained .*memberhp=1725.0$'
    fi
    if covers 1038; then
        expect_re '^TEST impact frame=1038 weapon=Pkg11DelayedWarhead '
    fi
    if covers 1039; then
        expect_re '^TEST pkg11 state frame=1039 name=Pkg11Captain hp=550.0/800.0 '
    fi
    if covers 1042; then
        expect_re '^TEST projectile frame=1042 weapon=Pkg11SiegeLaunch .*warhead=Pkg11SiegeWarhead '
    fi
    if covers 1045; then
        expect_re '^TEST pkg11 state frame=1045 name=Pkg11Captain hp=800.0/800.0 '
    fi
    if covers 1092; then
        expect_re '^TEST pkg11 debris-velocity frame=1092 prefix=BuildingDamageBits westbound=[67]$'
    fi
    if covers 1098; then
        expect_re '^TEST damage frame=1098 weapon=Pkg11DelayedWarhead .*dealt=44.00'
    fi
    if covers 1101; then
        expect_re '^TEST pkg11 state frame=1101 name=Pkg11Guardhouse hp=2010.0/3000.0 '
    fi
}
