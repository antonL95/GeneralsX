check() {
    if ! grep -q ' create Player_1 FixturePriorityFighter ' "$out/continue.txt"; then
        return 0
    fi
    for frame in 50 90; do
        if covers "$frame"; then
            expect "TEST ai-data priority frame=$frame profile=FixturePriority default=17 target=73" || return 1
        fi
    done
    if covers 79; then
        expect 'TEST ai-data tree frame=79 present=1' || return 1
    fi
    if covers 90; then
        expect 'TEST ai-data tree frame=90 present=0' || return 1
    fi
    printf 'PASS authored attack priorities and buffered tree fire spread (%s)\n' "$phase"
}
