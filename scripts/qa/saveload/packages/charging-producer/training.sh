prepare() {
    awk 'BEGIN {
        for (frame = 218; frame <= 410; frame++)
            print frame, "charging-producer-sample Player_1 MordorMumakil 0"
        for (frame = 415; frame <= 530; frame++)
            print frame, "charging-producer-sample Player_1 HaradMumakilMatriarch 0"
    }'
}

check() {
    if covers 223; then
        expect 'TEST frame 223: power SpecialAbilityChargeNoAim button Command_ChargeAttackHarad' || return 1
    fi
    if covers 428; then
        expect 'TEST frame 428: power SpecialAbilityMumakilGloriousCharge button Command_SpecialAbilityMumakilGloriousCharge' || return 1
    fi
    if covers 500; then
        expect 'TEST charging-producer 500 HaradMumakilMatriarch active=0 rider8=1' || return 1
    fi

    awk -v first="$first" -v end="$end" '
        function requireSample(frame, object, active) {
            if (frame >= first && frame <= end && (samples[frame] != object " active=" active || duplicate[frame])) {
                printf "Expected one %s CHARGING=%d sample before frame %d, got %s\n", object, active, frame, samples[frame] > "/dev/stderr"
                failed = 1
            }
        }
        BEGIN {
            shots[224] = shots[278] = shots[332] = 0
        }
        /^TEST charging-producer / {
            duplicate[$3] = ($3 in samples)
            samples[$3] = $4 " " $5
        }
        /^TEST timed weapon / && $6 == "weapon=MordorMumakilHeroEffect" &&
            $5 ~ /\((MordorMumakil|HaradMumakilMatriarch)\)$/ {
            split($4, stamp, "=")
            if ($5 !~ /\(MordorMumakil\)$/ || !(stamp[2] in shots) || shots[stamp[2]]++) {
                print "Unexpected charging weapon shot: " $0 > "/dev/stderr"
                failed = 1
            }
        }
        END {
            for (frame = 218; frame <= 410; frame++)
                requireSample(frame, "MordorMumakil", frame >= 225 && frame <= 374)
            for (frame = 415; frame <= 530; frame++)
                requireSample(frame, "HaradMumakilMatriarch", 0)
            for (frame in shots)
                if (frame >= first && frame <= end && shots[frame] != 1) {
                    print "Missing charging weapon shot at frame " frame > "/dev/stderr"
                    failed = 1
                }
            exit failed
        }
    ' "$log"
}
