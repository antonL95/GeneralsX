check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST script-hide-ui frame=$frame script=1 matches=1 letterbox=0" || return 1
    done
    if covers 246; then
        expect "TEST script-hide-ui frame=246 script=0 matches=1 letterbox=0" || return 1
    fi
}
