prepare() {
    printf '%s snapshot-path-queue Player_1 GondorFighter 0\n' "$save_frame"
    printf '%s snapshot-path-state Player_1 GondorFighter 0\n' "$save_frame" "$((save_frame + 1))"
}

check() {
    expect "TEST save-load path-state frame=$save_frame waiting=1"
    expect "TEST save-load path-state frame=$((save_frame + 1)) waiting=0"
}

compare() {
    phase=save log="$out/save.log" expect "TEST save-load path-queue frame=$save_frame queued=1"
    phase=continue log="$out/continue.log" expect "TEST save-load path-queue frame=$save_frame queued=1"
}
