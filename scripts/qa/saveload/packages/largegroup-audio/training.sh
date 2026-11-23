check_decision() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing large group audio decision in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if ! covers 962; then
        return 0
    fi

    event='group=goblinvoxambiencesinglesloop key=Goblin_Archer'
    sound='event=GoblinVoxAmbienceSinglesLoop'
    check_decision "kind=start $event weight=10 $sound cell=[0-9]+,(28|29) " || return 1
    check_decision "kind=below $event weight=2 $sound cell=[0-9]+,(22|23)$" || return 1
    owner=$(sed -n "s/.*kind=start $event weight=10 $sound .*owner=\([0-9]*\).*/\1/p" "$log" | head -n 1)
    check_decision "kind=handoff $event weight=10 $sound .*owner=$owner remaining=120 " || return 1
    if grep -Eq "kind=(start|handoff) $event .* $sound cell=[0-9]+,(22|23) " "$log"; then
        printf 'The below threshold army started a loop in %s\n' "$phase" >&2
        return 1
    fi

    source='group=smallgroupcelebrating key=Goblin_Archer'
    sound='event=Goblin_SmallGroup_CheerLoop'
    check_decision "kind=duck $source .* $sound .*gain=0 " || return 1
    check_decision "kind=duck $source .* $sound .*gain=1 " || return 1
    awk '
        /^LARGE GROUP AUDIO / && /kind=speed-limit group=smallgroupcelebrating / &&
            /event=Goblin_SmallGroup_CheerLoop / && /remaining=0 / {
            split(substr($NF, 5), pos, ",")
            frame = substr($4, 7) + 0
            if (previous && frame == previous + 1) {
                distance = sqrt((x - pos[1])^2 + (y - pos[2])^2 + (z - pos[3])^2)
                if (distance > 1.502 || distance < 1.498)
                    invalid = 1
                ++steps
            }
            x = pos[1]
            y = pos[2]
            z = pos[3]
            previous = frame
        }
        END {
            if (!steps || invalid) {
                print "Expected successive steps of 45 world units per second" > "/dev/stderr"
                exit 1
            }
        }
    ' "$log"
}
