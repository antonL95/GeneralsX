check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST script-deselect frame=$frame before=1 after=0" || return 1
    done
}
