check() {
    grep -q "horde-contact-member" "$out/commands.txt" || return 0
    failed=0
    for sample in 220:1 270:1 309:1 310:0 311:1 325:1 402:1 415:0; do
        frame=${sample%:*}
        active=${sample#*:}
        covers "$frame" || continue
        if ! grep -Fx "TEST horde-contact $frame active=$active" "$log"; then
            printf 'Expected horde contact active=%s at frame %s\n' "$active" "$frame"
            failed=1
        fi
    done
    return "$failed"
}
