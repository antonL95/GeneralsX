check() {
    [ -f "$out/commands.txt" ] || return 1
    if ! grep -q ' create Player_1 P63StoreCached ' "$out/commands.txt"; then
        return 0
    fi
    for frame in 250 290 465; do
        covers "$frame" || continue
        awk -v frame="$frame" '
            $1 == "TEST" && $2 == "locomotor-store" && $3 == frame {
                print
                if ($4 == "P63StoreCached")
                    cached = $5 == 0 && (frame != 465 || ($6 > 1720 && $6 < 1740 && $7 > 2655 && $7 < 2675))
                if ($4 == "P63StoreControl")
                    control = $5 == 1 && (frame != 465 || ($6 > 1720 && $6 < 1740 && $7 < 2655))
                if ($4 == "P63StoreLate")
                    late = $5 == 0 && (frame != 465 || ($6 > 1720 && $6 < 1740 && $7 > 2715 && $7 < 2725))
            }
            END { exit !(cached && control && late) }
        ' "$log" || return 1
    done
}
