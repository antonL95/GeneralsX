prepare() {
    awk 'BEGIN {
        for (frame = 280; frame <= 450; ++frame)
            print frame, "summon-busy-probe Player_1 MordorBalrog 0 ordered 3140 800"
        for (frame = 645; frame <= 740; ++frame)
            print frame, "summon-busy-probe Player_1 MordorBalrog 0 control"
    }'
}

check() {
    if ! covers 740; then
        return 0
    fi
    set -- "$log"
    if [ "$first" -ne 0 ]; then
        set -- "$out/save.log" "$log"
    fi
    awk '
        /^TEST summon-busy egg / {
            death[$4] = value($5)
        }
        /^TEST summon-busy sample / {
            label = $4
            age = value($7)
            birth[label] = value($6)
            busy[label, age] = value($8)
            idle[label, age] = value($9)
            x[label, age] = value($10)
            y[label, age] = value($11)
            seen[label, age] = 1
        }
        /^TEST summon-busy move ordered / {
            move = value($5)
            ++orders
        }
        function value(field) {
            return substr(field, index(field, "=") + 1) + 0
        }
        function fail(message) {
            print "summon-busy: " message > "/dev/stderr"
            error = 1
        }
        END {
            randomDelay = birth["ordered"] - death["ordered"]
            fixedDelay = birth["control"] - death["control"]
            printf "summon-busy random hatch=%d fixed hatch=%d first order=%d\n", randomDelay, fixedDelay, move
            if (!death["ordered"] || randomDelay < 84 || randomDelay > 156 || randomDelay % 6)
                fail("expected the random hatch on a retail tick between 35 and 65 percent")
            if (!death["control"] || fixedDelay != 30)
                fail("expected half of eleven retail ticks rounded down to five")
            if (orders != 1 || move != birth["ordered"] + 1 || !busy["ordered", 1] ||
                !seen["ordered", 6] || busy["ordered", 6] || !seen["ordered", 24] ||
                x["ordered", 1] - x["ordered", 24] <= 0.1)
                fail("expected the first player move to interrupt busy time and move toward the order")
            if (!busy["control", 1] || !busy["control", 70] || idle["control", 70] ||
                x["control", 1] != x["control", 70] || y["control", 1] != y["control", 70] ||
                !seen["control", 75] || busy["control", 75] || !idle["control", 75])
                fail("expected the unordered summon to stay busy through the authored hold and then idle")
            exit error
        }
    ' "$@"
}
