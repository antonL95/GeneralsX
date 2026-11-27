check() {
    if covers 5; then
        grep -E '^PW cp frame=5 used=[0-9]+ total=200 maximum=1000$' "$log" || {
            printf 'Expected the default skirmish command point rules at frame 5\n' >&2
            return 1
        }
    fi
    if covers 125; then
        grep -E '^TEST crate frame=[0-9]+ crate=FollowupTreasureControl .*reward=resources resources=100 levels=0 banner=0$' "$log" &&
        grep -E '^TEST crate frame=[0-9]+ crate=FollowupTreasureZero .*reward=resources resources=100 levels=0 banner=0$' "$log"
    fi
}
