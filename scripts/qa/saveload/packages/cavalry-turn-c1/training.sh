prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 235; frame <= 419 && frame <= end; ++frame) {
            print frame, "cavalry-c1-probe Player_1 CavalryC1Horde 0"
            print frame, "cavalry-c1-probe Player_1 CavalryC1Hero 0"
        }
        for (frame = 620; frame <= 920 && frame <= end; ++frame)
            print frame, "cavalry-c1-quarter-probe Player_1 CavalryC1Quarter 0"
    }'
}

check() {
    [ "$phase" = continue ] && covers 419 || return 0
    awk -f "$package_dir/turn.awk" "$log" || return 1
    if covers 920; then
        awk -f "$package_dir/quarter.awk" "$log"
    fi
}

compare() {
    covers 419 || return 0
    for run in continue resume; do
        awk -v save="$save_frame" '$1 == "TEST" && ($2 == "cavalry-c1" || $2 == "cavalry-c1-quarter") && $3 >= save' \
            "$out/$run.log" > "$out/cavalry-c1-$run.txt"
    done
    diff -u "$out/cavalry-c1-continue.txt" "$out/cavalry-c1-resume.txt"
}
