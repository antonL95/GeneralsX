check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST ui-ini-credits frame=$frame presets=1 default=1500" || return 1
    done
}
