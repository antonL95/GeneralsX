check() {
    if ! grep -q '^95 create Player_2 SkirmishLargeDefenseAnchor ' "$out/continue.txt" || ! covers 124; then
        return 0
    fi
    awk -v phase="$phase" '
        FNR == NR && /^TEST frame 95: create SkirmishLargeDefenseAnchor / {
            split($8, id, "=")
            anchor[id[2]] = 1
        }
        FNR == NR { next }
        $1 == "AI_DEFENSE" && $2 == 124 && $3 == 4 && $4 in anchor {
            seen++
            if ($6 != 3 || $7 != 0 || $8 != 3) {
                print "Expected three recruits for a large cluster with six registered combatants in " phase ": " $0
                bad = 1
            }
        }
        END {
            if (seen != 1) print "Missing large defense at frame 124 in " phase
            if (seen == 1 && !bad) print "PASS large defense recruits half the roster (three of six, " phase ")"
            exit seen != 1 || bad
        }
    ' "$out/continue.log" "$log"
}
