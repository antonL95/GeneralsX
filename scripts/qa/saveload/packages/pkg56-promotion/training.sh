check_promotion() {
    if covers "$1"; then
        expect "PKG56 promotion $1 rank=$2 cheer=$3 tint=0 leveled=${4:-0}"
    fi
}

check_saved_state() {
    frame=$1
    if covers "$frame"; then
        expect "PKG56 saved $frame GondorFaramir rank=2 xp=98.500 start=1 first=1 second=1 creation=1 objectGrant=1 members=0 promoted=0 banner=0 cheering=0" || return
        expect "PKG56 saved $frame GondorFighterHorde rank=2 xp=73.500 start=1 first=1 second=1 creation=1 objectGrant=0 members=16 promoted=16 banner=1 cheering=15" || return
        expect "PKG56 taskmaster $frame rank=2 profile=1 class=1 subclass=1 appearance=1"
    fi
}

check() {
    if covers 217; then
        expect 'PKG56 parser 217 match=1' || return
    fi
    if grep -q '^219 pkg56-creation ' "$out/continue.txt"; then
        if covers 219; then
            expect 'PKG56 creation 219 0' || return
            expect 'PKG56 creation 220 1' || return
            expect 'PKG56 taskmaster setup 222 valid=1' || return
        fi
        check_saved_state 242 || return
        check_saved_state 245 || return
    fi
    check_promotion 242 1 0 || return
    check_promotion 243 2 0 || return
    check_promotion 245 2 1 || return
    check_promotion 249 2 1 || return
    check_promotion 280 2 0 || return
    check_promotion 282 3 0 1 || return
    check_promotion 370 3 0 1 || return
    check_promotion 372 3 0
}
