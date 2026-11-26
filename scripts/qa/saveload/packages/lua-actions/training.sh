check() {
    if covers 246; then
        grep -E '^TEST lua action camera frame=246 clip=[0-9]+\.[0-9]+ letterbox=0$' "$log" || return 1
        if grep -q '^TEST lua action camera frame=246 clip=5.00 ' "$log"; then
            return 1
        fi
    fi
    if covers 247; then
        expect 'TEST lua action frame=247 probe=Begin ok=1 letterbox=1 dead=0' || return 1
    fi
    if covers 248; then
        expect 'TEST lua action frame=248 probe=End ok=1 letterbox=0 dead=0' || return 1
    fi
    if covers 250; then
        expect 'TEST lua action frame=250 probe=Kill ok=1 letterbox=0 dead=1' || return 1
    fi
    if covers 258; then
        expect 'TEST lua action camera frame=258 clip=5.00 letterbox=0' || return 1
    fi
}
