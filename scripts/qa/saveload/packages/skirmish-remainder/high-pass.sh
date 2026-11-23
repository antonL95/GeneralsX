check() {
    if covers 364; then
        awk '
            /^TEST frame 342: create SkirmishOpportunityBuilder for Player_1 .*2740.0, 3070.0/ {
                split($8, id, "=")
                target = id[2]
            }
            $1 == "AI_WAVE" && $2 == 364 && $3 == 4 && $4 == "opportunity" && $6 == target { sent++ }
            END {
                if (!target || sent != 1) {
                    print "Missing opportunity wave at frame 364 for the exposed builder " target
                    exit 1
                }
            }
        ' "$log" || return 1
    fi
}

compare() {
    if [ "$end" -lt 424 ] || [ "$save_frame" -gt 424 ]; then
        return
    fi
    for phase in continue resume; do
        awk '
            FNR == NR && /^TEST frame 342: create SkirmishOpportunityBuilder for Player_1 .*2740.0, 3070.0/ {
                split($8, id, "=")
                target = id[2]
            }
            $1 == "AI_OPPORTUNITY_HELD" && $2 == 424 && $3 == 4 && $4 == target && $5 == 1 { found = 1 }
            END {
                if (!found) print "Opportunity team no longer attached to builder " target " at frame 424"
                exit !found
            }
        ' "$out/continue.log" "$out/$phase.log" || return 1
    done
}
