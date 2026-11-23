check() {
    if covers 64; then
        awk '
            /^TEST frame 45: create GondorFarm for Player_1 .*2180.0, 3050.0/ {
                split($8, id, "=")
                target = id[2]
            }
            $1 == "AI_WAVE" && $2 == 64 && $3 == 4 && $4 == "structure" && $6 == target { sent++ }
            END {
                if (!target || sent != 1) {
                    print "Missing offensive wave for target " target " at frame 64"
                    exit 1
                }
            }
        ' "$log" || return 1
    fi
}

compare() {
    ini="$out/data/GeneralsX/GeneralsZH/Maps/map mp high pass/map.ini"
    expiry=0
    if [ -f "$ini" ] && grep -Fq '    SecondsTillTargetsCanExpire = 4' "$ini"; then
        expiry=184
    fi
    for phase in continue resume; do
        set -- "$out/continue.log"
        begin=0
        if [ "$phase" = resume ]; then
            begin=$save_frame
            set -- "$@" "$out/resume.log"
        fi
        awk -v begin="$begin" -v end="$end" -v expiry="$expiry" -v prefix="$out/continue.log" '
            function covered(frame) { return begin <= frame && frame <= end }
            FILENAME == prefix && /^TEST frame 45: create GondorFarm for Player_1 .*2180.0, 3050.0/ {
                split($8, id, "=")
                target = id[2]
            }
            FILENAME == prefix && /^TEST frame 95: create Pkg46DefenseAnchor/ {
                split($8, id, "=")
                anchor[id[2]] = 1
            }
            FILENAME == prefix && /^TEST frame 100: create Pkg46DefenseAttacker/ {
                split($8, id, "=")
                attacker = id[2]
            }
            FILENAME != prefix || ARGC == 2 {
                if ($1 == "AI_DEFENSE" && $2 == 124 && $3 == 4 && $4 in anchor && $5 == attacker && $6 == 2 && $8 > 0)
                    defended = 1
                if ($1 == "AI_TARGET_EXPIRE" && $2 == expiry && $3 == 4 && $5 == target)
                    expired = 1
            }
            END {
                if (covered(124) && !defended) {
                    print "Missing medium defense response at frame 124 against " attacker
                    bad = 1
                }
                if (expiry && covered(expiry) && !expired) {
                    print "Missing target " target " expiry at frame " expiry
                    bad = 1
                }
                exit bad
            }
        ' "$@" || return 1
    done
}
