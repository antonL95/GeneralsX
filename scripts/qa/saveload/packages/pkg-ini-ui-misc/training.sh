check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST ui-ini-credits frame=$frame presets=1 default=1500" || return 1
        expect "TEST ui-ini-placement-claim frame=$frame color=ffffffff" || return 1
        expect "TEST ui-ini-transition-images frame=$frame world=1 solo=1" || return 1
        expect "TEST ui-ini-fonts frame=$frame sacha=1 fixed=0 courier=0" || return 1
        expect "TEST ui-ini-button-border frame=$frame altered=1" || return 1
        expect "TEST ui-ini-post-load-sound frame=$frame active=1" || return 1
    done
}
