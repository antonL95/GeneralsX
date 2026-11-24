sample_start=208
sample_end=348

prepare() {
    awk -v start="$sample_start" -v stop="$sample_end" -v end="$end" 'BEGIN {
        for (frame = start; frame <= stop && frame <= end; ++frame) {
            print frame, "flamesprayer-state Player_1 RhunFlamesprayer 0"
            print frame, "flamesprayer-state Player_1 MorgulBurghash 0"
        }
    }'
}

check_actor() {
    awk -v actor="$1" -v weapon="$2" -v toggle_on="$3" -v toggle_off="$4" \
        -v charge_on="$5" -v charge_off="$6" -v shots="$7" -v first="$first" -v end="$end" \
        -v sample_start="$sample_start" -v sample_end="$sample_end" '
        BEGIN {
            count = split(shots, frames, " ")
            for (i = 1; i <= count; ++i)
                if (frames[i] >= first && frames[i] <= end)
                    expected_shot[frames[i]] = 1
        }
        /^TEST flamesprayer / && $4 == "source=" actor {
            frame = substr($3, 7) + 0
            toggle = (frame >= toggle_on && frame < toggle_off) + 0
            charging = (frame >= charge_on && frame < charge_off) + 0
            if ($5 != "toggle=" toggle || $6 != "level2=" (1 - toggle) ||
                $7 != "level3=" toggle || $8 != "charging=" charging || seen[frame]++) {
                print "Unexpected sample: " $0
                failed = 1
            }
        }
        /^TEST timed weapon / && $6 == "weapon=" weapon {
            frame = substr($4, 7) + 0
            if (index($5, "(" actor ")") == 0 || !expected_shot[frame]) {
                print "Unexpected shot: " $0
                failed = 1
            } else {
                delete expected_shot[frame]
            }
        }
        END {
            for (frame = sample_start; frame <= sample_end && frame <= end; ++frame) {
                if (frame >= first && !seen[frame]) {
                    print actor ": missing state at frame " frame
                    failed = 1
                }
            }
            for (frame in expected_shot) {
                print weapon ": missing shot at frame " frame
                failed = 1
            }
            exit failed
        }
    ' "$log"
}

check() {
    check_actor RhunFlamesprayer FlamesprayerBoilerExceedSelfDamage 224 284 225 309 "228 258 288" &&
        check_actor MorgulBurghash BurghashImmolationSelfDamage 229 290 230 314 "229 259 289"
}
