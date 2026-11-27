check() {
    for frame in 243 244 320; do
        if covers "$frame"; then
            expect "TEST castle-area-backfill frame=$frame points=18 active=1 inside=1 cached=1 captured=1"
        fi
    done
    if covers 405; then
        expect 'TEST castle-area-backfill frame=405 points=0 active=0 inside=0 cached=0 captured=1'
    fi
}

run_legacy_phase() {
    GENERALSX_TEST_COMMANDS="$out/castle-area-$1.txt" timeout --signal=INT --kill-after=10 360 \
        env -u HOME gdb -q -nx -batch -return-child-result -x "$2" -ex run \
        --args "$PKGS1_BUILD_DIR/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" \
        -loadsave "$3" > "$out/castle-area-$1.log" 2>&1 || {
        tail -40 "$out/castle-area-$1.log" >&2
        return 1
    }
}

compare() {
    if [ "$save_frame" -lt 243 ] || [ "$save_frame" -ge 330 ]; then
        return
    fi

    legacy_frame=$((save_frame + 5))
    cat "$out/resume.gdb" "$package_dir/write-version-17.gdb" > "$out/castle-area-write.gdb"
    printf '%s\n' \
        "$save_frame castle-area-backfill-forget * CastleAreaBackfillCamp 0" \
        "$((legacy_frame - 1)) castle-area-backfill-probe * CastleAreaBackfillCamp 0" \
        "$legacy_frame snapshot-save - - 0 castle-area-v17.sav" \
        "$((legacy_frame + 1)) quit - - 0" > "$out/castle-area-write.txt"
    run_legacy_phase write "$out/castle-area-write.gdb" parity.sav
    phase=castle-area-write
    log="$out/$phase.log"
    expect "TEST castle-area-backfill frame=$((legacy_frame - 1)) points=0 active=0 inside=0 cached=0 captured=1"
    expect 'TEST castle-area-backfill legacy logic version=17'
    expect "TEST snapshot-save frame=$legacy_frame code=0"

    printf '%s\n' \
        "$legacy_frame castle-area-backfill-probe * CastleAreaBackfillCamp 0" \
        "$((legacy_frame + 1)) quit - - 0" > "$out/castle-area-read.txt"
    run_legacy_phase read "$out/resume.gdb" castle-area-v17.sav
    phase=castle-area-read
    log="$out/$phase.log"
    expect "TEST castle-area-backfill frame=$legacy_frame points=18 active=1 inside=1 cached=1 captured=1"
    modern_points=$(sed -n 's/^TEST castle-area-backfill geometry frame=243 //p' "$out/continue.log")
    legacy_points=$(sed -n "s/^TEST castle-area-backfill geometry frame=$legacy_frame //p" "$log")
    if [ -z "$modern_points" ] || [ "$modern_points" != "$legacy_points" ]; then
        printf 'Restored castle perimeter points differ at frame %s\n' "$legacy_frame" >&2
        return 1
    fi
}
