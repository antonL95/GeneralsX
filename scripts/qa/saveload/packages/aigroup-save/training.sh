check() {
    if covers 240; then
        expect "TEST aigroup-save order frame=240 attack-move=1" || return 1
    fi
    if covers 244; then
        grep -E '^TEST aigroup-save group frame=244 id=[1-9][0-9]* [0-9]+ [0-9]+$' "$log" || return 1
    fi
    if covers 249; then
        grep -E '^TEST aigroup-save group frame=249 id=[1-9][0-9]* [0-9]+$' "$log"
    fi
}

compare() {
    for phase in continue resume; do
        awk -v cutoff="$save_frame" '
            /^TEST aigroup-save group / && substr($4, 7) + 0 >= cutoff { print }
        ' "$out/$phase.log" > "$out/aigroup-save-$phase.txt"
    done
    diff -u "$out/aigroup-save-continue.txt" "$out/aigroup-save-resume.txt"
}
