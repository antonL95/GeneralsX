native() {
    grep -q hero-unlock-native "$out/$phase.txt"
}

state_line() {
    grep -E "^TEST hero-unlock frame=$1 set=[A-Za-z_]+ hero=$2 " "$log" | head -n 1
}

check() {
    native || return 0
    if covers 5; then
        expect 'TEST hero-unlock-native frame=5 faction=FactionMirkwood' || return 1
    fi
    if covers 12; then
        expect 'TEST hero-unlock-upgrade frame=12 upgrade=Upgrade_Arkenstone complete=0' || return 1
        state_line 12 MirkwoodGrimbeorn | grep -q ' listed=1 slot=1 recruitable=0 alive=0 queued=0$' || {
            printf 'Grimbeorn was not locked before the conditions\n' >&2
            return 1
        }
        state_line 12 MirkwoodGalion | grep -q ' listed=1 slot=3 recruitable=1 alive=0 queued=0$' || {
            printf 'Galion was not recruitable at the start\n' >&2
            return 1
        }
    fi
    if covers 24; then
        state_line 24 MirkwoodGrimbeorn | grep -q ' queued=0$' || {
            printf 'A locked Grimbeorn was queued\n' >&2
            return 1
        }
    fi
    if covers 1960; then
        grep -E '^TEST hero-unlock-upgrade frame=1[0-9]00 upgrade=Upgrade_Arkenstone complete=1$' "$log" > /dev/null || {
            printf 'Upgrade_Arkenstone was never granted to the player\n' >&2
            return 1
        }
        state_line 1900 MirkwoodGrimbeorn | grep -q ' slot=3 recruitable=1 ' || {
            printf 'Grimbeorn was not recruitable after the conditions\n' >&2
            return 1
        }
    fi
    if covers 2004; then
        state_line 2004 MirkwoodGrimbeorn | grep -q ' queued=1$' || {
            printf 'Grimbeorn was not queued through the production command\n' >&2
            return 1
        }
    fi
    if covers 4400; then
        state_line 4400 MirkwoodGrimbeorn | grep -q ' alive=1 queued=0$' || {
            printf 'Grimbeorn did not emerge from the fortress\n' >&2
            return 1
        }
    fi
}
