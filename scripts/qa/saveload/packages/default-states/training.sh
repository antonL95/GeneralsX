default_state() {
    if ! covers "$1"; then
        return 0
    fi
    expect "TEST default-states $1 0 $2"
}

check() {
    default_state 230 'DefaultStatesTower stealthed=0 camo=0 trees=0 selectable=1 idle=1' || return 1
    default_state 230 'DefaultStatesSentries stealthed=0 camo=0 trees=0 selectable=1 idle=1' || return 1
    default_state 300 'GuldurRuinousTower stealthed=1 camo=1 trees=0 selectable=1 idle=1' || return 1
    default_state 300 'DefaultStatesTower stealthed=0 camo=0 trees=0 selectable=1 idle=1' || return 1
    default_state 420 'DefaultStatesSentries stealthed=1 camo=1 trees=1 selectable=1 idle=1' || return 1
    default_state 420 'DefaultStatesTower stealthed=0 camo=0 trees=0 selectable=1 idle=1' || return 1
    grep -q default-cast "$out/$phase.txt" || return 0
    if covers 306; then
        expect 'TEST default-cast 306 Command_SpellBookDragonStrike ready=1' || return 1
    fi
    default_state 310 'SpellBookDragonStrikeDragon stealthed=0 camo=0 trees=0 selectable=0 idle=0' || return 1
    if covers 340; then
        expect 'TEST default-order 340 selectable=0' || return 1
    fi
    default_state 1440 absent || return 1
}

compare() {
    for trace_phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 ~ /^default-/ && $3 >= first' \
            "$out/$trace_phase.log" > "$out/default-states-$trace_phase.trace"
    done
    diff -u "$out/default-states-continue.trace" "$out/default-states-resume.trace"
}
