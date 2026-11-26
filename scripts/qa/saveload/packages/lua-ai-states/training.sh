check() {
    if covers 230; then
        expect 'TEST lua ai frame=230 probe=ProbeAlert ok=1 alert=1 fear=0' || return 1
    fi
    for frame in 231 252; do
        if covers "$frame"; then
            expect "TEST lua chant frame=$frame member=0 horde=0" || return 1
        fi
    done
    for frame in 243 245; do
        if covers "$frame"; then
            expect "TEST lua chant frame=$frame member=1 horde=1" || return 1
        fi
    done
    if covers 246; then
        grep -E '^Lua: Object [0-9]+ \[LuaAIStateProbe, owned by player ' "$log" || return 1
        grep -F 'TEST lua ai frame=246 probe=ProbeDescription ok=1 ' "$log" || return 1
    fi
    if covers 247; then
        expect 'TEST lua ai frame=247 probe=ProbeFear ok=1 alert=0 fear=1' || return 1
    fi
}
