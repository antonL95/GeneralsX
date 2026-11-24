check() {
    if ! grep -q '^45 create PlyrCreeps SkirmishRingBearer ' "$out/continue.txt"; then
        return 0
    fi
    awk -v phase="$phase" -v first="$first" -v end="$end" '
        FNR == NR && /^TEST frame 45: create SkirmishRingBearer / {
            split($8, id, "=")
            target = id[2]
        }
        FNR == NR { next }
        $1 == "AI_WAVE" && $3 == 4 && $4 == "opportunity" && $6 == target {
            if ($2 == 124 && $5 == 3) sent++
            else bad = 1
        }
        $1 == "AI_OPPORTUNITY_HELD" && $3 == 4 && $4 == target && $5 == 1 { held++ }
        END {
            if ((first <= 124 && 124 <= end && sent != 1) || (first <= 184 && 184 <= end && !held) || bad) {
                print "Missing EndGame wave or held tactical slot for Ring bearer " target " (" phase ")"
                exit 1
            }
            print "PASS EndGame Ring bearer remains an opportunity target (" phase ")"
        }
    ' "$out/continue.log" "$log"
}
