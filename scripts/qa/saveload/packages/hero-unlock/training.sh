native() {
    grep -q hero-unlock-native "$out/$phase.txt"
}

grimbeorn() {
    grep -Fxq "TEST hero-unlock frame=$1 set=$2 hero=MirkwoodGrimbeorn $3" "$log" || {
        printf 'Frame %s: expected set=%s %s\n' "$1" "$2" "$3" >&2
        grep -E "^TEST hero-unlock frame=$1 " "$log" >&2
        return 1
    }
}

check() {
    native || return 0
    locked='listed=1 slot=1 recruitable=0 alive=0 queued=0'
    unlocked=MirkwoodFortressCommandSet_Grimbeorn
    if covers 5; then
        expect 'TEST hero-unlock-native frame=5 faction=FactionMirkwood' || return 1
    fi
    if covers 10; then
        expect 'TEST hero-unlock-upgrade frame=10 upgrade=Upgrade_Arkenstone complete=0' || return 1
        grimbeorn 10 MirkwoodFortressCommandSet "$locked" || return 1
    fi
    if covers 16; then
        grimbeorn 16 MirkwoodFortressCommandSet "$locked" || return 1
    fi
    if covers 30; then
        grimbeorn 30 MirkwoodFortressCommandSet "$locked" || return 1
    fi
    if covers 2100; then
        expect 'TEST hero-unlock-upgrade frame=2100 upgrade=Upgrade_Arkenstone complete=1' || return 1
        grimbeorn 2100 "$unlocked" 'listed=1 slot=3 recruitable=1 alive=0 queued=0' || return 1
    fi
    if covers 3000; then
        grimbeorn 3000 "$unlocked" 'listed=1 slot=2 recruitable=1 alive=0 queued=1' || return 1
    fi
    if covers 4600; then
        expect 'TEST hero-unlock-upgrade frame=4600 upgrade=Upgrade_Arkenstone complete=1' || return 1
        grimbeorn 4600 "$unlocked" 'listed=1 slot=1 recruitable=1 alive=1 queued=0' || return 1
    fi
}
