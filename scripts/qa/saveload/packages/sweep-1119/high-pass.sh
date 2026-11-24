prepare() {
    if [ -n "${SWEEP1119_SCENARIO:-}" ]; then
        awk -v end="$end" 'BEGIN {
            for (frame = 101; frame <= end; ++frame)
                print frame, "log Player_1 MirkwoodFortress 0"
        }'
    fi
}

check() {
    [ -n "${SWEEP1119_SCENARIO:-}" ] || return 0
    if [ "$phase" = resume ] && [ "$SWEEP1119_TERRAIN_VERSION" -ne 0 ]; then
        expect "TEST sweep 1119 wrote TerrainLogic v$SWEEP1119_TERRAIN_VERSION"
    fi
    if covers 70; then
        expect 'TEST snapshot devastation frame=70 payout=1080 ready=0'
    fi
    if covers 81; then
        expect 'TEST snapshot devastation frame=81 payout=0 ready=0'
    fi
    if covers 82; then
        expect 'TEST snapshot devastation frame=82 payout=720 ready=0'
    fi
    if covers 12; then
        grep -E '^TEST frame 12: check MirkwoodStatue canMake 0 possible 1 legal 0 order 0 ai 0 .* at 1010\.0 381\.3 ' "$log"
    fi
    if covers 20; then
        grep -E '^TEST frame 20: check MirkwoodStatue canMake 0 possible 1 legal 3 order 3 ai 3 .* at 1010\.0 381\.3 ' "$log"
    fi
    if covers 30; then
        grep -E '^TEST frame 30: check MirkwoodStatue canMake 0 possible 1 legal 0 order 0 ai 0 .* at 1010\.0 381\.3 ' "$log"
        expect 'TEST frame 30: command set SellableCommandSet'
    fi
    if covers 36; then
        grep -E '^TEST frame 36: object [0-9]+ MirkwoodStatue pos 1010\.000 381\.300 ' "$log"
    fi
    if covers 83; then
        grep -E '^TEST frame 83: object [0-9]+ MirkwoodStatue pos 1010\.000 381\.300 ' "$log"
    fi
}

compare() {
    if [ -z "${SWEEP1119_SCENARIO:-}" ] || ! covers 680; then
        return 0
    fi
    for run_phase in continue resume; do
        awk '
            /^TEST frame [0-9]+: object [0-9]+ MirkwoodFortress pos 282\.600 522\.700 / && $NF > 0 {
                frame = $3 + 0
                printf "Mirkwood first progress at frame %d, %d frames after the order\n", frame, frame - 100
                exit
            }
            END { if (!frame) exit 1 }
        ' "$out/$run_phase.log" > "$out/$run_phase.mirkwood"
    done
    diff -u "$out/continue.mirkwood" "$out/resume.mirkwood"
    cat "$out/continue.mirkwood"
}
