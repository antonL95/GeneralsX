step() {
    covers "$2" || return 0
    expect "$3" >/dev/null || {
        printf 'FAIL ring chain step %s (%s)\n' "$1" "$phase" >&2
        grep -E '^TEST ringchain' "$log" >&2
        return 1
    }
    printf 'PASS ring chain step %s (%s)\n' "$1" "$phase"
}

state() {
    printf 'TEST ringchain frame=%s %s' "$1" "$2"
}

check() {
    if ! grep -q ' ringchain-gollum Player_1 RingEvilGollum ' "$out/continue.txt"; then
        return 0
    fi
    step gollum-spawn 60 "$(state 60 'rings=0 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    step ring-drop 100 "$(state 100 'rings=1 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    step right-click-pickup 110 'TEST ringchain-order frame=110 target=ring command=pickup' &&
    step carrier 240 "$(state 240 'rings=1 holder=RingEvilCarrier upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    step carrier-death 270 "$(state 270 'rings=1 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    step pickup-near-corpse 400 "$(state 400 'rings=1 holder=RingEvilCarrier upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    step right-click-fortress 410 'TEST ringchain-order frame=410 target=fortress command=enter' &&
    step delivery 950 "$(state 950 'rings=0 holder=- upgrade=1 fortress-upgrade=1 fortress-flag=1 recruitable=1 hero=0')" &&
    step recruit-order 960 'TEST ringchain-recruit frame=960 queued=1' &&
    step hero-spawn 6500 "$(state 6500 'rings=0 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=1')" &&
    step hero-death 6550 "$(state 6550 'rings=1 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0')" &&
    for event in DiscoveredRing RingPickedUpLocal LocalPlayerLosesRing; do
        step "eva-$event" 400 "TEST ringchain-eva frame=400 event=$event played=1" || return 1
    done &&
    step eva-LocalPlayerGainsRing 970 "TEST ringchain-eva frame=970 event=LocalPlayerGainsRing played=1"
}
