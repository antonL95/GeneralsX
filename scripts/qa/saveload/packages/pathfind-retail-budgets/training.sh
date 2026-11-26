check() {
    grep -q "create Player_1 RetailPathBudgetProbe" "$out/commands.txt" || return 0
    for frame in 235 255; do
        if covers "$frame"; then
            expect "TEST pathfind-budget $frame adjacent found=1" || return 1
            expect "TEST pathfind-budget $frame distant found=0" || return 1
        fi
    done
}
