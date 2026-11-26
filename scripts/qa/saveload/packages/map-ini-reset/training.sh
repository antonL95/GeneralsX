prepare() {
    cp "$package_dir/existing.ini" "$out/map-ini-existing.ini"
}

check() {
    grep -q 'map-ini-reset ' "$out/commands.txt" || return 0
    if covers 200; then
        expect 'TEST map-ini first player=777 particle=42.0 fire=777'
        expect 'TEST map-ini extras first player=1 particle=1'
    fi
    if covers 245; then
        expect 'TEST map-ini saved player=777 particle=42.0 fire=777'
        expect 'TEST map-ini extras saved player=1 particle=1'
    fi
    if covers 242; then
        expect 'TEST map-ini eva first played=1'
    fi
    if covers 202; then
        expect 'TEST map-ini existing first played=0'
    fi
    if covers 247; then
        expect 'TEST map-ini eva saved played=1'
    fi
    if covers 280; then
        expect 'TEST map-ini second player=0 particle=-1.0 fire=100'
        expect 'TEST map-ini extras second player=0 particle=0'
        expect 'TEST map-ini existing-particle second wind=2.0'
    fi
    if covers 248; then
        expect 'TEST map-ini existing-particle override wind=42.0'
    fi
    if covers 282; then
        expect 'TEST map-ini eva second played=0'
        expect 'TEST map-ini existing second played=1'
    fi
}
