check() {
    for frame in 60 120; do
        if covers "$frame"; then
            expect "TEST ai-retaliate-friends frame=$frame radius=60.00" || return 1
        fi
    done
    printf 'PASS authored RetaliateFriendsRadius loads across reload (%s)\n' "$phase"
}
