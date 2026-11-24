check() {
    if ! grep -q '^45 create Player_2 SkirmishRingCourier ' "$out/continue.txt"; then
        return 0
    fi
    if covers 75; then
        expect 'TEST ringstate frame=75 player=0 citadel=0 carrier=0' || return 1
    fi
    if covers 180; then
        expect 'TEST ringstate frame=180 player=0 citadel=0 carrier=1' || return 1
    fi
    awk -v phase="$phase" '
        FNR == NR && /^TEST frame 45: create SkirmishRingCourier / {
            split($8, id, "=")
            courier = id[2]
        }
        FNR == NR && /^TEST frame 45: create TheDroppedRing / {
            split($8, id, "=")
            ring = id[2]
        }
        FNR == NR { next }
        $1 == "AI_TARGET" && $3 == 4 && $4 == "collect" && $5 == courier && $6 == ring {
            if ($2 == 124) collected++
            else bad = 1
        }
        $1 == "AI_RING_RETURN" && $3 == 4 && $4 == courier && $2 == 184 { delivered++ }
        END {
            if ((phase == "continue" && (collected != 1 || delivered != 1)) || bad) {
                print "Missing EndGame pickup or return order for courier " courier " and Ring " ring " (" phase ")"
                exit 1
            }
        }
    ' "$out/continue.log" "$log" || return 1
    if covers 580; then
        expect 'TEST ringstate frame=580 player=1 citadel=1 carrier=0' || return 1
        printf 'PASS the EndGame courier picks up and deposits the Ring (%s)\n' "$phase"
    fi
}
