prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 185; frame <= 370 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestEagle 0 PlyrCreeps:SwoopTestTarget:0"
        for (frame = 447; frame <= 632 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestFellBeast 0 PlyrCreeps:SwoopTestTarget:0"
        for (frame = 709; frame <= 920 && frame <= end; ++frame)
            print frame, "giant-bird-swoop-trace Player_1 SwoopTestEagle 0 PlyrCreeps:GondorTrebuchetDebrisPiece:0"
    }'
}

check_swoop_toss() {
    local label="$1" bird="$2" start_frame="$3" end_frame="$4"
    awk -v label="$label" -v bird="$bird" -v start_frame="$start_frame" -v end_frame="$end_frame" '
        /^TEST giant-bird-swoop / {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            frame = sample["frame"] + 0
            if (frame < start_frame || frame > end_frame)
                next
            if (sample["swoop"] == "1" && !dive_start)
                dive_start = frame
            if (sample["shot"] + 0 > 0 && !shot)
                shot = sample["shot"] + 0
            if (sample["carrier"] == bird && !grab_frame)
                grab_frame = frame
            if (grab_frame && !release_frame && sample["carrier"] == "-") {
                release_frame = frame
                release_freefall = sample["freefall"] + 0
                release_impact = sample["impact"] + 0
                release_tvx = sample["tvx"] + 0
                release_tvy = sample["tvy"] + 0
                release_tvz = sample["tvz"] + 0
                release_tz = sample["tz"] + 0
            }
        }
        END {
            printf "%s dive=%u shot=%u grab=%u release=%u freefall=%u impact=%u tv=(%.3f,%.3f,%.3f) tz=%.3f\n",
                label, dive_start, shot, grab_frame, release_frame, release_freefall, release_impact,
                release_tvx, release_tvy, release_tvz, release_tz
            ok = dive_start > 0 && shot > dive_start && grab_frame > 0 && release_frame > grab_frame &&
                release_freefall == 1 && release_impact == 1 && release_tvx > 0 && release_tvz < 0
            exit !ok
        }
    ' "$log"
}

check() {
    if covers 370; then
        check_swoop_toss "eagle" "SwoopTestEagle" 185 370 || return
    fi
    if covers 632; then
        check_swoop_toss "fellbeast" "SwoopTestFellBeast" 447 632 || return
    fi
    if covers 920; then
        awk '
            /^TEST giant-bird-swoop / {
                for (i = 3; i <= NF; ++i) {
                    split($i, field, "=")
                    sample[field[1]] = field[2]
                }
                frame = sample["frame"] + 0
                if (frame < 709 || frame > 920)
                    next
                if (sample["passenger"] == "GondorTrebuchetDebrisPiece" && !debris_grab)
                    debris_grab = frame
                if (debris_grab && !debris_release && sample["contain"] == "0" && sample["target"] == "GondorTrebuchetDebrisPiece") {
                    debris_release = frame
                    debris_freefall = sample["freefall"] + 0
                    debris_tvz = sample["tvz"] + 0
                }
            }
            END {
                printf "debris grab=%u release=%u freefall=%u tvz=%.3f\n",
                    debris_grab, debris_release, debris_freefall, debris_tvz
                exit !(debris_grab > 0 && debris_release > debris_grab && debris_freefall == 1 && debris_tvz < 0)
            }
        ' "$log" || return
    fi
}

compare() {
    for phase_name in continue resume; do
        awk -v frame="$save_frame" '/^TEST giant-bird-swoop frame=/ && substr($3, 7) + 0 > frame' \
            "$out/$phase_name.log" > "$out/giant-bird-swoop-$phase_name.trace"
    done
    cmp "$out/giant-bird-swoop-continue.trace" "$out/giant-bird-swoop-resume.trace"
}
