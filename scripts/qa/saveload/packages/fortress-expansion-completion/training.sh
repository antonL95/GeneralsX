prepare() {
    frame=29
    while [ "$frame" -le 965 ]; do
        printf '%s fortress-expansion-state Player_1 MenFortressExpansionPadSide 0\n' "$frame"
        frame=$((frame + 1))
    done
}

check() {
    if covers 244; then
        grep -Eq '^TEST fortress-expansion 244 pad=1 id=[1-9][0-9]* percent=[0-9.]+ under=1 dead=0$' "$log" ||
            { printf '%s\n' 'The plot expansion was not under construction at frame 244.'; return 1; }
    fi
    if covers 965; then
        grep -Eq '^TEST fortress-expansion 965 pad=1 id=[1-9][0-9]* percent=[0-9.-]+ under=0 dead=0$' "$log" ||
            { printf '%s\n' 'The plot expansion did not complete by frame 965.'; return 1; }
    fi
}
