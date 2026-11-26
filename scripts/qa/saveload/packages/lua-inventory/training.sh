check() {
    if covers 218; then
        expect 'TEST lua player-upgrade frame=218 found=0' || return 1
    fi
    for frame in 239 246; do
        if covers "$frame"; then
            expect "TEST lua player-upgrade frame=$frame found=1" || return 1
        fi
    done
    for frame in 240 247; do
        if covers "$frame"; then
            expect "TEST check-upgrade frame=$frame found=1" || return 1
        fi
    done
    for frame in 243 245; do
        if covers "$frame"; then
            awk -v frame="$frame" '$1 == "TEST" && $2 == "lua" && $3 == "visibility" && $4 == "frame=" frame {
                print
                if ($6 != "hidden=0" && $6 != "hidden=1") invalid = 1
                hidden[$5] = ($6 == "hidden=1")
            }
            END {
                exit !(!invalid && length(hidden) == 4 &&
                    hidden["part=HOOD"] != hidden["part=HOODDOWN"] &&
                    hidden["part=BODY"] != hidden["part=BODYDUSTER"])
            }' "$log" || return 1
        fi
    done
}

compare() {
    before=$(sed -n 's/^TEST lua visibility frame=243 //p' "$out/continue.log")
    after=$(sed -n 's/^TEST lua visibility frame=245 //p' "$out/resume.log")
    [ "$before" = "$after" ]
}
