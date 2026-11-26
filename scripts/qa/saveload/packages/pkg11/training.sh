expect_re() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if covers 209; then
        expect_re '^TEST pkg11 state frame=209 name=Pkg11Horde .*memberhp=1725.0$'
        expect_re '^TEST pkg11 state frame=209 name=Pkg11HordeControl .*memberhp=1785.0$'
        expect 'TEST pkg11 pulse frame=209 name=Pkg11Horde combat=1 delay=30'
        expect 'TEST pkg11 pulse frame=209 name=Pkg11HordeControl combat=1 delay=60'
    fi
    if covers 212; then
        expect_re '^TEST pkg11 state frame=212 name=Pkg11Horde .*items=15 .*memberhp=1785.0$'
        expect_re '^TEST pkg11 state frame=212 name=Pkg11HordeNoContained .*memberhp=1725.0$'
    fi
    if covers 238; then
        expect_re '^TEST impact frame=238 weapon=Pkg11DelayedWarhead '
    fi
    if covers 239; then
        expect_re '^TEST pkg11 state frame=239 name=Pkg11Captain hp=550.0/800.0 '
    fi
    if covers 240; then
        expect_re '^TEST pkg11 state frame=240 name=Pkg11Guardhouse hp=2615.0/3000.0 state=0 front=0 '
    fi
    if covers 242; then
        expect_re '^TEST projectile frame=242 weapon=Pkg11SiegeLaunch .*warhead=Pkg11SiegeWarhead '
    fi
    if covers 243; then
        expect_re '^TEST pkg11 held frame=243 id=[1-9][0-9]* name=EntThrownBuildingRock$'
        expect_re '^TEST pkg11 state frame=243 name=Pkg11Wall hp=1910.0/2400.0 .*grab=490,16,0,1 '
    fi
    if covers 244; then
        expect_re '^TEST pkg11 held frame=244 id=[1-9][0-9]* name=EntThrownBuildingRock$'
    fi
    if covers 245; then
        expect_re '^TEST pkg11 held frame=245 id=[1-9][0-9]* name=EntThrownBuildingRock$'
        expect_re '^TEST pkg11 state frame=245 name=Pkg11Captain hp=800.0/800.0 '
    fi
    if covers 251; then
        expect_re '^TEST pkg11 state frame=251 name=Pkg11Guardhouse hp=2230.0/3000.0 state=0 front=1 '
        expect_re '^TEST pkg11 state frame=251 name=Pkg11GuardhouseControl hp=2230.0/3000.0 state=0 front=0 '
    fi
    if covers 252; then
        expect 'TEST pkg11 debris frame=252 prefix=BuildingGBBarracksN2D2BitA count=1'
    fi
    if covers 292; then
        expect_re '^TEST pkg11 debris-velocity frame=292 prefix=BuildingDamageBits westbound=[67]$'
    fi
    if covers 298; then
        expect_re '^TEST damage frame=298 weapon=Pkg11DelayedWarhead .*dealt=44.00'
    fi
    if covers 301; then
        expect_re '^TEST pkg11 state frame=301 name=Pkg11Guardhouse hp=2010.0/3000.0 '
    fi
    if covers 326; then
        expect_re '^TEST pkg11 state frame=326 name=Pkg11Guardhouse .*state=2 front=0 '
    fi
    if covers 328; then
        expect_re '^TEST pkg11 state frame=328 name=Pkg11Guardhouse .*state=1 front=0 '
    fi
    if covers 384; then
        expect_re '^TEST pkg11 state frame=384 name=Pkg11Cheerer .*cheer=0 '
    fi
    if covers 385; then
        expect_re '^TEST emotion frame=385 unit=Pkg11Cheerer type=CHEER source=Pkg11Militia '
    fi
    if covers 386; then
        expect_re '^TEST pkg11 state frame=386 name=Pkg11Cheerer .*cheer=1 '
    fi
    if covers 470; then
        expect_re '^TEST pkg11 state frame=470 name=Pkg11EntOak .*items=0 '
        expect_re '^TEST pkg11 state frame=470 name=Pkg11WallControl hp=2400.0/2400.0 .*grab=0,0,0,0 '
    fi
}

compare() {
    for frame in 244 245; do
        if [ "$frame" -ge "$save_frame" ] && [ "$frame" -le "$end" ]; then
            held=$(grep -E "^TEST pkg11 held frame=$frame id=[1-9][0-9]* name=EntThrownBuildingRock$" "$out/continue.log")
            grep -Fx "$held" "$out/resume.log" > /dev/null || {
                printf 'Mismatch in resume for: %s\n' "$held" >&2
                return 1
            }
        fi
    done
}
