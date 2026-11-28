check_prefix() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if covers 157; then
        check_prefix '^TEST castle-wildlife status frame=157 name=CastleWildlifeWalker inside=0 '
        check_prefix '^TEST castle-wildlife status frame=157 name=CastleWildlifeAnimal inside=0 moving=1 '
    fi
    for frame in 243 244; do
        if covers "$frame"; then
            check_prefix "^TEST castle-wildlife status frame=$frame name=CastleWildlifeWalker inside=1 "
            check_prefix "^TEST castle-wildlife status frame=$frame name=CastleWildlifeAnimal inside=0 moving=1 "
            animal_dist=$(sed -n "s/^TEST castle-wildlife status frame=$frame name=CastleWildlifeAnimal inside=0 moving=1 dist=//p" "$log" | tail -n 1)
            if [ -z "$animal_dist" ] || [ "$animal_dist" -lt 220 ]; then
                printf 'Expected CastleWildlifeAnimal dist >= 220 at frame %s, got %s\n' "$frame" "${animal_dist:-missing}" >&2
                return 1
            fi
        fi
    done
    if covers 380; then
        check_prefix '^TEST castle-wildlife status frame=380 name=CastleWildlifeWalker inside=0 '
        animal_dist=$(sed -n 's/^TEST castle-wildlife status frame=380 name=CastleWildlifeAnimal inside=0 moving=[01] dist=//p' "$log" | tail -n 1)
        if [ -z "$animal_dist" ] || [ "$animal_dist" -lt 270 ]; then
            printf 'Expected CastleWildlifeAnimal dist >= 270 at frame 380, got %s\n' "${animal_dist:-missing}" >&2
            return 1
        fi
    fi
}
