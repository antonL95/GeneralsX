prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 235; frame <= 419 && frame <= end; ++frame) {
            print frame, "cavalry-c1-probe Player_1 CavalryC1Horde 0"
            print frame, "cavalry-c1-probe Player_1 CavalryC1Hero 0"
        }
    }'
}

check() {
    [ "$phase" = continue ] && covers 419 || return 0
    awk -f "$package_dir/turn.awk" "$log"
}

compare() {
    covers 419 && [ "$save_frame" -le 419 ] || return 0
    for run in continue resume; do
        awk -v save="$save_frame" '$1 == "TEST" && $2 == "cavalry-c1" && $3 >= save' "$out/$run.log" > "$out/cavalry-c1-$run.txt"
    done
    diff -u "$out/cavalry-c1-continue.txt" "$out/cavalry-c1-resume.txt"
}
