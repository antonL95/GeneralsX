expect_portal() {
    if covers "$1" && ! grep -E "TEST siege-portal frame=$1 object=$2 $3" "$log"; then
        printf 'Missing siege-portal state for %s at %s in %s\n' "$2" "$1" "$phase" >&2
        return 1
    fi
}

check() {
    expect_portal 220 SiegePortalLadder 'active=1 docked=1 .* waypoints=4 topPos=1'
    expect_portal 250 SiegePortalHorde 'ord=0 .* climbing=1 .* climbSeg=0 '
    expect_portal 300 SiegePortalHorde 'ord=0 .* climbing=1 .* climbSeg=1 '
    expect_portal 400 SiegePortalHorde 'ord=0 layer=15 wall=1 .* contained=0 climbing=0 '
    expect_portal 422 SiegePortalDefender 'ord=0 layer=15 wall=1 .* inRange=1 topRange=1 '
    expect_portal 422 SiegePortalDefender 'ord=1 layer=15 wall=1 .* inRange=0 topRange=0 '
    if covers 422 && covers 520; then
        hp_before=$(grep -E "TEST siege-portal frame=422 object=SiegePortalLadder " "$log" | sed -n 's/.* hp=\([0-9.]*\)\/.*/\1/p' | head -n 1)
        hp_after=$(grep -E "TEST siege-portal frame=520 object=SiegePortalLadder " "$log" | sed -n 's/.* hp=\([0-9.]*\)\/.*/\1/p' | head -n 1)
        if [ -z "$hp_before" ] || [ -z "$hp_after" ] || ! awk -v a="$hp_after" -v b="$hp_before" 'BEGIN { exit !(a < b) }'; then
            printf 'Expected SiegePortalLadder HP drop from %s to %s in %s\n' "$hp_before" "$hp_after" "$phase" >&2
            return 1
        fi
    fi
}
