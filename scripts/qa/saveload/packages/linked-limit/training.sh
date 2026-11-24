check() {
    [ "$save_frame" -eq 2450 ] || return 0
    if covers 263; then
        grep -E '^TEST pkg16 button frame=263 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=1$' "$log" || return 1
    fi
    if covers 275; then
        grep -E '^TEST pkg16 button frame=275 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=0$' "$log" || return 1
        grep -E '^TEST frame 276: produce NoldorWarriorHordeGilGalad on object [0-9]+ queued=0$' "$log" || return 1
        [ "$(grep -Ec '^TEST production 280 RivendellDormitory [0-9]+ NoldorWarriorHorde(GilGalad)? ' "$log")" -eq 2 ] || return 1
    fi
    if covers 2400; then
        grep -E '^TEST pkg16 button frame=2400 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=0$' "$log" || return 1
        grep -E '^TEST frame 2401: produce NoldorWarriorHordeGilGalad on object [0-9]+ queued=0$' "$log" || return 1
    fi
    for frame in 2425 2465 2498; do
        if covers "$frame"; then
            [ "$(grep -Ec "^TEST horde anchor $frame [0-9]+ count [0-9]+ selected 1$" "$log")" -eq 2 ] || return 1
        fi
    done
}
