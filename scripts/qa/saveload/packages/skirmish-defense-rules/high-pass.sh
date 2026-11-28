prepare() {
    if grep -q '^95 create Player_2 SkirmishDefenseRuleAnchor ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
        printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
    fi
}

check() {
    if ! grep -q '^95 create Player_2 SkirmishDefenseRuleAnchor ' "$out/continue.txt" || ! covers 424; then
        return 0
    fi
    awk -v phase="$phase" '
        FNR == NR && /^TEST frame 95: create SkirmishDefenseRuleAnchor / {
            split($8, id, "=")
            anchor = id[2]
        }
        FNR == NR { next }
        $1 == "AI_DEFENSE_ROLL" && $3 == 4 && $4 == anchor {
            if (++rolls[$2] != 1) {
                print "Small defense rolled twice at frame " $2 " (" phase ")"
                bad = 1
            }
            passed[$2] = ($5 < 30)
            rejected += ($5 >= 30)
        }
        $1 == "AI_DEFENSE" && $3 == 4 && $4 == anchor {
            if ($6 != 1 || $7 != 0 || $8 != 1) {
                print "Unexpected small defense recruitment in " phase ": " $0
                bad = 1
            }
            sent[$2] = 1
        }
        END {
            for (frame in rolls) {
                if (passed[frame] != (frame in sent)) {
                    print "Small defense order disagreed with its roll at frame " frame " (" phase ")"
                    bad = 1
                }
                admitted += passed[frame]
            }
            if (!rejected || !admitted) {
                print "Expected accepted and rejected small defense rolls in " phase
                bad = 1
            }
            if (!bad) print "PASS fixed small defense roll and one recruit (" phase ")"
            exit bad
        }
    ' "$out/continue.log" "$log"
}
