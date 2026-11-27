check() {
    if covers 239; then
        expect "TEST script-world-lights frame=239 normal=1 fading=0 dimmed=0" || return 1
    fi
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST script-world-lights frame=$frame normal=0 fading=1 dimmed=0" || return 1
    done
    if covers 275; then
        expect "TEST script-world-lights frame=275 normal=0 fading=0 dimmed=1" || return 1
    fi
}
