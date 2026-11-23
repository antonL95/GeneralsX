check() {
    if covers 244; then
        grep -E '^TEST fortress-expansion 244 pad=1 id=[1-9][0-9]* percent=[1-9][0-9]?\.[0-9]{2} under=1 dead=0$' "$log" ||
            { printf '%s\n' "Expected a live plot expansion with construction progress at frame 244 ($phase)."; return 1; }
    fi
    if covers 1000; then
        grep -E '^TEST fortress-expansion 1000 pad=1 id=[1-9][0-9]* percent=-1\.00 under=0 dead=0$' "$log" ||
            { printf '%s\n' "Expected a live, completed plot expansion at frame 1000 ($phase)."; return 1; }
    fi
}
