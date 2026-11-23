prepare() {
    for frame in "$((save_frame + 1))" "$((save_frame + 10))"; do
        printf '%s pkg09-state Player_1 TreeCoverRemovedProbe 0\n' "$frame"
        printf '%s pkg09-state Player_1 TreeCoverStandingProbe 0\n' "$frame"
    done
}

expect_cover() {
    frame=$1
    probe=$2
    hidden=$3
    prefix="TEST pkg09 frame=$frame name=$probe hidden=$hidden "
    grep -F "$prefix" "$log" || {
        printf 'Missing in %s: %s\n' "$phase" "$prefix" >&2
        return 1
    }
}

check() {
    if [ "$phase" = resume ] && [ -n "${TREE_COVER_LEGACY_GDB:-}" ]; then
        expect "TEST tree-cover wrote TerrainLogic v3 with only harvestable trees"
    fi
    if covers 30; then
        expect_cover 30 TreeCoverRemovedProbe 1
        expect_cover 30 TreeCoverStandingProbe 1
    fi
    for observation in 220 "$((save_frame + 1))" "$((save_frame + 10))"; do
        if covers "$observation"; then
            expect_cover "$observation" TreeCoverRemovedProbe 0
            expect_cover "$observation" TreeCoverStandingProbe 1
        fi
    done
}
