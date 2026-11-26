expect_climb() {
    if covers "$1" && ! grep -E "TEST horde-climb frame=$1 object=$2 $3" "$log"; then
        printf 'Missing horde-climb state for %s at %s in %s\n' "$2" "$1" "$phase" >&2
        return 1
    fi
}

check() {
    expect_climb 220 HordeClimbLadder 'deployed=1 .* release=1'
    expect_climb 228 HordeClimbHorde '.* accepted=1 '
    expect_climb 278 HordeClimbHorde '.* accepted=1 '
    expect_climb 350 HordeClimbLadder 'deployed=1 .* release=1'
    expect_climb 678 HordeClimbHorde 'layer=15 wall=1 .* contained=0 .* members=10 wallMembers=10 sameHorde=10 held=0'
    if covers 735; then
        count=$(grep -cE "TEST horde-climb frame=735 object=HordeClimbHorde layer=15 wall=1 .* contained=0 .* members=10 wallMembers=10 sameHorde=10 held=0" "$log" || true)
        if [ "$count" -ne 2 ]; then
            printf 'Expected 2 HordeClimbHorde on wall at 735 in %s, got %s\n' "$phase" "$count" >&2
            return 1
        fi
    fi
}
