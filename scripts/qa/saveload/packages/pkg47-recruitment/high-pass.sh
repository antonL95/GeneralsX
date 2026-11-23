check() {
    if covers 180; then
        expect "TEST pkg47 target frame=180 owner=human source=Pkg47ChangedArcher target=Pkg47Far" || return 1
        expect "TEST pkg47 target frame=180 owner=human source=Pkg47ControlArcher target=Pkg47Near" || return 1
        expect "TEST pkg47 target frame=180 owner=computer source=Pkg47ChangedArcher target=Pkg47Far" || return 1
    fi
    awk -F '\t' -v first="$first" -v end="$end" '
        $1 == "AI_UNIT_PRIORITY" && $3 == 4 {
            seen[$4]++
            if ($4 == 0 && ($6 != 0.3 || $7 != 100 || $8 != 150)) bad = 1
            if ($4 == 1 && ($6 != 0.6 || $7 != 200 || $8 != 150)) bad = 1
            if ($4 == 2 && ($6 != 0.9 || $7 != 300 || $8 != 150)) bad = 1
            base = $4 == 0 ? 500 : $8
            adjusted = base + ($6 - $5) * $7
            cap = 1800 + (1 - $5) * 300
            if (adjusted < $8) adjusted = $8
            if (adjusted > cap) adjusted = cap
            if (adjusted < base) adjusted = base
            if ($9 - adjusted > 0.01 || adjusted - $9 > 0.01) bad = 1
        }
        END {
            if (first <= 64 && end >= 64 && !seen[0]) bad = 1
            if (first <= 184 && end >= 184 && !seen[1]) bad = 1
            if (first <= 304 && end >= 304 && !seen[2]) bad = 1
            exit bad
        }' "$log"
}
