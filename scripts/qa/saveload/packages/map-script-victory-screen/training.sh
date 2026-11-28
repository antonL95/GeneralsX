check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST script-victory frame=$frame handled=1 shown=0 ending=0 disabled=1 deselected=1" || return 1
    done
}
