check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST named-cameras frame=$frame parsed=1 rejected=1" || return 1
    done
}
