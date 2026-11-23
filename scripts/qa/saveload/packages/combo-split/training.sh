check_members() {
    before=$1
    source=$2
    after=$3
    unit=$4
    count=$5
    destination=${6:-}
    if awk -v before="$before" -v source="$source" -v after="$after" -v unit="$unit" \
        -v count="$count" -v destination="$destination" '
        $2 == "combo-member" && $3 == before && $4 == source && $6 == unit { ids[$7] = 1 }
        $2 == "combo-member" && $3 == after && $4 == destination && $6 == unit { moved[$7] = 1 }
        $2 == "combo-loose" && $3 == after && $4 == unit && destination == "" { moved[$5] = 1 }
        END {
            for (id in ids) {
                if (!(id in moved)) exit 1
                found++
            }
            exit found != count
        }
    ' "$log"; then
        return
    fi
    printf 'Frame %s did not retain %s %s members from %s in %s\n' "$after" "$count" "$unit" "$source" "${destination:-the world}" >&2
    return 1
}

check_axe_variants() {
    variants=$(awk '
        $2 == "combo-member" && $3 == 1260 && $4 == "ComboSplitTestWildmen" && $6 ~ /^IsengardWildmanAxe[12]$/ {
            if (!first) first = $6
            count[$6]++
        }
        END {
            if (count["IsengardWildmanAxe1"] + count["IsengardWildmanAxe2"] != 5) exit 1
            other = first == "IsengardWildmanAxe1" ? "IsengardWildmanAxe2" : "IsengardWildmanAxe1"
            print first, count[first], other, count[other] + 0
        }
    ' "$log")
    set -- $variants
    expect "TEST combo-state 1262 IsengardWildmanAxeHorde 0 $1 $2"
    check_members 1260 ComboSplitTestWildmen 1262 "$1" "$2" IsengardWildmanAxeHorde
    check_members 1260 ComboSplitTestWildmen 1262 "$3" "$4"
}

check() {
    if covers 238; then
        expect "TEST combo-casualties 238 ComboSplitTestPikesOnly IsengardUrukCrossbow 5"
        expect "TEST combo-casualties 238 ComboSplitTestSpearsOnly IsengardWildmanAxe 5"
    fi
    if covers 1252; then
        expect "TEST combo-state 1252 ComboSplitTestPikes 0 absent"
        expect "TEST combo-state 1252 IsengardFighterHorde 0 absent"
        expect "TEST combo-state 1252 IsengardUrukCrossbowHorde 0 IsengardUrukCrossbow 5"
        check_members 1250 ComboSplitTestPikes 1252 IsengardPikeman 15
        check_members 1250 ComboSplitTestPikes 1252 IsengardUrukCrossbow 5 IsengardUrukCrossbowHorde
    fi
    if covers 1262; then
        expect "TEST combo-state 1262 ComboSplitTestWildmen 0 absent"
        expect "TEST combo-state 1262 IsengardWildmanSpearHorde 0 absent"
        check_members 1260 ComboSplitTestWildmen 1262 IsengardWildmanSpear 15
        check_axe_variants
    fi
    if covers 1272; then
        expect "TEST combo-state 1272 ComboSplitTestPikesOnly 0 absent"
        expect "TEST combo-state 1272 IsengardFighterHorde 0 absent"
        expect "TEST combo-state 1272 IsengardUrukCrossbowHorde 1 absent"
        check_members 1270 ComboSplitTestPikesOnly 1272 IsengardPikeman 15
    fi
    if covers 1284; then
        expect "TEST combo-state 1284 ComboSplitTestSpearsOnly 0 absent"
        expect "TEST combo-state 1284 IsengardWildmanSpearHorde 0 absent"
        expect "TEST combo-state 1284 IsengardWildmanAxeHorde 1 absent"
        check_members 1282 ComboSplitTestSpearsOnly 1284 IsengardWildmanSpear 15
    fi
}
