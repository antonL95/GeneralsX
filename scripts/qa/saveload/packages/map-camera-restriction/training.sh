check() {
    if covers 242; then
        expect "TEST camera-restriction-arm frame=242 inside=1" || return 1
    fi
    if covers 245; then
        expect "TEST camera-restriction-check frame=245 inside=1" || return 1
        expect "TEST camera-restriction-slide frame=245 clamp=1 slide=1 probe=1" || return 1
    fi
    if covers 246; then
        expect "TEST camera-restriction-check frame=246 inside=0" || return 1
    fi
}
