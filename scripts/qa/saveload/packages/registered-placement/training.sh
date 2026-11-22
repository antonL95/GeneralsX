prepare() {
    if [ "$save_frame" -gt 230 ]; then
        printf '%s snapshot-obstacle-count Player_1 SaveLoadStampOnComplete 0\n' "$save_frame"
        if [ "$end" -gt "$save_frame" ]; then
            printf '%s snapshot-obstacle-count Player_1 SaveLoadStampOnComplete 0\n' "$((save_frame + 1))"
        fi
    fi
}

check() {
    for obstacle_frame in 262 286; do
        if covers "$obstacle_frame"; then
            expect "TEST save-load obstacle frame=$obstacle_frame template=CaHRestlessDeadBarrow ownsCell=1"
        fi
    done
}

compare() {
    if [ "$save_frame" -gt 230 ]; then
        for run_phase in continue resume; do
            grep -E '^TEST save-load obstacle-count frame=[0-9]+ template=SaveLoadStampOnComplete cells=[1-9][0-9]*$' \
                "$out/$run_phase.log" > "$out/$run_phase.obstacles"
        done
        expected=1
        if [ "$end" -gt "$save_frame" ]; then
            expected=2
        fi
        test "$(wc -l < "$out/continue.obstacles")" -eq "$expected"
        diff -u "$out/continue.obstacles" "$out/resume.obstacles"
        cat "$out/continue.obstacles"
    fi
}
