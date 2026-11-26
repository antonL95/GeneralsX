check() {
    grep -q "create Player_1 PkgPathBudgetGuard" "$out/commands.txt" || return 0
    if covers 200; then
        awk '$1 == "TEST" && $2 == "pos" && $3 == 200 && $5 == 0 { seen = 1; if ($6 < 900 || $6 > 920 || $7 < 390 || $7 > 410) exit 1 } END { if (!seen) exit 1 }' "$log" || return 1
    fi
}
