check_beast() {
    frame=$1
    label=$2
    state=$3
    if covers "$frame" && ! grep -Fq "TEST beast-rampage frame=$frame case=$label $state " "$log"; then
        printf 'Unexpected beast state at frame %s for %s (expected %s)\n' "$frame" "$label" "$state"
        grep -F "TEST beast-rampage frame=$frame case=$label " "$log" || :
        return 1
    fi
}

check() {
    if covers 209; then
        expect 'TEST beast-rampage seat frame=209 rider=0 admitted=1' || return 1
        expect 'TEST beast-rampage seat frame=209 rider=1 admitted=1' || return 1
    fi
    check_beast 235 crew-before-fire 'aflame=0 rampage=0 running=0 passengers=1' || return 1
    for sample in 239 250; do
        check_beast "$sample" bound-empty 'aflame=1 rampage=1 running=0 passengers=0 enraged-locomotor=1' || return 1
        check_beast "$sample" bound-crewed 'aflame=1 rampage=1 running=0 passengers=1 enraged-locomotor=1' || return 1
        check_beast "$sample" dry 'aflame=0 rampage=0 running=0 passengers=0' || return 1
        check_beast "$sample" unbound-crewed 'aflame=1 rampage=0 running=0 passengers=1' || return 1
        check_beast "$sample" unbound-empty 'aflame=1 rampage=0 running=0 passengers=0' || return 1
    done
    check_beast 280 ejected 'aflame=1 rampage=1 running=0 passengers=0' || return 1
    check_beast 298 running-off 'aflame=1 rampage=0 running=1 passengers=0'
}
