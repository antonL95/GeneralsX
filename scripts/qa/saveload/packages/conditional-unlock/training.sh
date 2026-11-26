unlock() {
    if covers "$1"; then
        expect "TEST unlock frame=$1 button=Command_ConstructWildColdDrake listed=1 $2"
    fi
}

check() {
    unlock 10 'upgrades=0 available=0 owned=0'
    unlock 950 'upgrades=0 available=0 owned=0'
    unlock 1950 'upgrades=1 available=1 owned=0'
    unlock 3900 'upgrades=1 available=0 owned=1'
}
