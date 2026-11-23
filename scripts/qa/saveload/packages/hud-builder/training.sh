check() {
    for frame in 35 748; do
        if covers "$frame"; then
            expect "TEST hud-builder $frame selectable=1 masked=0 hidden=0"
        fi
    done
    for frame in 243 245 430; do
        if covers "$frame"; then
            expect "TEST hud-builder $frame selectable=0 masked=1 hidden=1"
        fi
    done
    if covers 243; then
        grep -E '^TEST pkg36 site frame=243 name=HudBuilderFarm .* progress=[1-9][0-9]*\.[0-9] under=1 ' "$log"
    fi
    if covers 748; then
        grep -E '^TEST pkg36 site frame=748 name=HudBuilderFarm .* under=0 ' "$log"
    fi
}
