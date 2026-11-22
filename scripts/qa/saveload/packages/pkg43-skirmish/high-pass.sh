check() {
    if covers 4; then
        awk -F '\t' '$1 == "AI_BUILD" && $2 == 4 && $5 == "MordorSlaughterHouse_ForAI" && $7 == 1 && $8 == 1 { found = 1 }
            END { exit !found }' "$log" || return 1
    fi
    awk -F '\t' '$1 == "AI_BUILD" && $8 == 1 && $7 > $8 { exit 1 }' "$log" || return 1
    for frame in 20 120; do
        if covers "$frame"; then
            expect "TEST pkg43 difficulty frame=$frame level=3 brutal-upgrade=1 cost=300 frames=450"
            expect "TEST pkg43 control-price frame=$frame cost=500 frames=900"
        fi
    done
}
