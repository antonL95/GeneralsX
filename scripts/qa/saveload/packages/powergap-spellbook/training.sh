check() {
    for frame in 230 231; do
        if covers $frame; then
            grep -q "^TEST powergap-cast frame=$frame button=.* issued=1\$" "$log" || {
                printf 'Missing in %s: cast at frame %s\n' "$phase" "$frame" >&2
                return 1
            }
        fi
    done
    for line in \
        '320 powergap-unit frame=320 unit=PowerGapTaintAlly modifier=1 taint=IsengardTaintLand hurt=0' \
        '320 powergap-unit frame=320 unit=PowerGapTaintOutside modifier=0 taint=- hurt=0' \
        '320 powergap-unit frame=320 unit=PowerGapTaintEnemy modifier=0 taint=IsengardTaintLand hurt=0' \
        '580 powergap-unit frame=580 unit=PowerGapFloodEnemy modifier=0 taint=- hurt=1' \
        '580 powergap-unit frame=580 unit=PowerGapFloodAlly modifier=0 taint=- hurt=0' \
        '580 powergap-unit frame=580 unit=PowerGapFloodOutside modifier=0 taint=- hurt=0' \
        '580 powergap-objects frame=580 template=SpellBookWrathOfBelegaer count=0 remaining=0' \
        '580 powergap-objects frame=580 template=WrathOfBelegaerDamageObject count=0 remaining=0'; do
        if covers "${line%% *}"; then
            expect "TEST ${line#* }" || return 1
        fi
    done
    if covers 580; then
        grep -Eq '^TEST powergap-objects frame=580 template=IsengardTaintLand count=1 remaining=86[0-9]{2}$' "$log" || {
            printf 'Missing in %s: taint patch alive with its authored lifetime at frame 580\n' "$phase" >&2
            return 1
        }
    fi
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 ~ /^powergap-/ { split($3, at, "="); if (at[2] >= first) print }' \
            "$out/$phase.log" > "$out/powergap-$phase.txt"
    done
    test -s "$out/powergap-continue.txt"
    diff -u "$out/powergap-continue.txt" "$out/powergap-resume.txt"
}
