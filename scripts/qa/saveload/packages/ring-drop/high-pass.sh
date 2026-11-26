check() {
    if [ "${GENERALSX_RING_DROP:-0}" = 0 ]; then
        ! grep -q '^RING-DROP ' "$log" || return 1
        for frame in 301 480; do
            if covers "$frame"; then
                expect "TEST ring-drop frame=$frame rings=0 near-fortress=0 holder=-"
            fi
        done
        return 0
    fi
    if covers 299; then
        expect "TEST ring-drop frame=299 rings=0 near-fortress=0 holder=-"
    fi
    if covers 300; then
        grep -q '^RING-DROP frame=300 object=' "$log" || return 1
    fi
    if covers 301; then
        expect "TEST ring-drop frame=301 rings=1 near-fortress=1 holder=-"
    fi
    if covers 480; then
        expect "TEST ring-drop frame=480 rings=1 near-fortress=1 holder=RingDropCarrier"
    fi
}
