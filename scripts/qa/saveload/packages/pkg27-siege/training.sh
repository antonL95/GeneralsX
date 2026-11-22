expect_state() {
    if covers "$1" && ! grep -E "TEST pkg27 frame=$1 object=$2 $3" "$log"; then
        printf 'Missing siege state for %s at %s in %s\n' "$2" "$1" "$phase" >&2
        return 1
    fi
}

check() {
    expect_state 186 Pkg27Ladder 'occupants=1 deployed=0 packing=0 unpacking=0 power=1 ready=1 '
    expect_state 200 Pkg27MordorSiegeTower 'occupants=2 '
    expect_state 200 Pkg27IthilienRam 'occupants=6 '
    expect_state 200 Pkg27Ladder 'occupants=1 deployed=0 packing=0 unpacking=1 power=1 ready=0 '
    expect_state 242 Pkg27Ladder 'occupants=0 deployed=1 packing=0 unpacking=0 power=1 ready=1 '
    expect_state 250 Pkg27Ladder 'occupants=0 deployed=1 packing=0 unpacking=0 power=1 ready=1 '
    expect_state 250 IsengardBeserker '.* wall=1$'
    expect_state 253 Pkg27Horde '.* power=1 .* accepted=1 '
    expect_state 275 Pkg27Ladder '.* approaching=1 .* accepted=1 goal=1312\.'
    expect_state 304 Pkg27Ladder 'occupants=0 deployed=0 packing=1 unpacking=0 power=1 ready=0 '
    expect_state 368 Pkg27Ladder 'occupants=0 deployed=0 packing=0 unpacking=0 power=1 ready=1 '
    expect_state 376 Pkg27Ladder '.* accepted=1 goal=1071\.'
    expect_state 355 Pkg27Keep 'occupants=1 .* accepted=1 '
    expect_state 240 Pkg27Demolisher 'occupants=-1 deployed=0 packing=0 unpacking=1 '
    expect_state 248 Pkg27Demolisher 'occupants=-1 deployed=0 packing=0 unpacking=1 '
    expect_state 270 Pkg27Demolisher 'occupants=-1 deployed=1 '
    expect_state 340 Pkg27Demolisher 'occupants=-1 deployed=0 packing=0 unpacking=0 '
}
