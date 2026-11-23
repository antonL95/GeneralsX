check() {
    for frame in 65 245; do
        if covers "$frame"; then
            grep -E "^PW cp frame=$frame used=[0-9]+ total=2000 maximum=2000$" "$log" || {
                printf 'Expected the map command point override at frame %s\n' "$frame" >&2
                return 1
            }
        fi
    done
}
