prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 580; frame <= 720 && frame <= end; ++frame) {
            print frame, "pkg36-site Player_1 PorterConstructionMill 0"
            print frame, "bcaravan-census Player_1 PorterConstructionBuilder 0"
        }
    }'
}

expect_pattern() {
    grep -E "$1" "$log" || {
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    }
}

check_completion() {
    awk '
        /^TEST pkg36 site frame=[0-9]+ name=PorterConstructionMill / {
            frame = substr($4, 7) + 0
            building[frame] = ($9 == "under=1")
            if (frame >= 580 && $9 == "under=0" && !complete)
                complete = frame
        }
        /^TEST bcaravan census frame=[0-9]+ template=PorterConstructionBuilder / {
            frame = substr($4, 7) + 0
            alive[frame] = ($6 == "living=1")
            if (frame >= 580 && $6 == "living=0" && !consumed)
                consumed = frame
        }
        END {
            if (!building[complete - 1] || !alive[consumed - 1] || consumed < complete || consumed > complete + 1) {
                printf "Expected both transitions at completion, got complete=%d consumed=%d\n", complete, consumed
                exit 1
            }
            printf "Porter construction completed at frame %d, consumption at frame %d\n", complete, consumed
        }
    ' "$log"
}

check() {
    for frame in 86 89 97; do
        if covers "$frame"; then
            expect "TEST hud-builder $frame selectable=1 masked=0 hidden=0"
        fi
    done
    if covers 89; then
        expect_pattern '^TEST pkg36 site frame=89 name=PorterConstructionMill .* progress=0\.0 under=1 '
        expect_pattern '^TEST pkg08 builder frame=89 target=PorterConstructionMill '
    fi
    if covers 90; then
        expect 'TEST frame 90: key stop'
    fi
    if covers 97; then
        expect 'TEST bcaravan census frame=97 template=PorterConstructionBuilder living=1'
        expect_pattern '^TEST pkg08 builder frame=97 target=- '
    fi
    if covers 99; then
        expect_pattern '^TEST hud-state 99 PorterConstructionBuilder .* selectable=1 hidden=0 selected=1 '
    fi
    for frame in 243 244 245; do
        if covers "$frame"; then
            expect "TEST hud-builder $frame selectable=0 masked=1 hidden=1"
            expect_pattern "^TEST pkg36 site frame=$frame name=PorterConstructionMill .* progress=([1-9][0-9]*\.[0-9]|0\.[1-9]) under=1 "
        fi
    done
    if covers 244; then
        expect_pattern '^TEST hud-state 244 PorterConstructionBuilder .* selectable=0 hidden=1 selected=0 '
        expect 'TEST frame 244: command set GuldurPorterCommandSet'
        expect 'TEST frame 244: slot 1 Command_EvilStop type 12 object -'
        expect 'TEST frame 244: slot 2 Command_SpecialAbilityGuldurShade type 0 object -'
        expect 'TEST frame 244: slot 3 Command_PorterConstructGuldurSlaveMill type 1 object GuldurSlaveMill'
        if grep -E '^TEST frame 244: slot ([3-9]|[1-9][0-9]+) .* type ([02-9]|[1-9][0-9]+) ' "$log"; then
            printf 'Unexpected nonconstruction command in the porter command set\n' >&2
            return 1
        fi
    fi
    if covers 580 && covers 720; then
        check_completion
    fi
}
