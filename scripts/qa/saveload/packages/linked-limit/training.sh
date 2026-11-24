check() {
    [ "$save_frame" -eq 4450 ] || return 0
    if covers 2263; then
        grep -E '^TEST pkg16 button frame=2263 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=1$' "$log" || return 1
    fi
    if covers 2275; then
        grep -E '^TEST pkg16 button frame=2275 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=0$' "$log" || return 1
        grep -E '^TEST frame 2276: produce NoldorWarriorHordeGilGalad on object [0-9]+ queued=0$' "$log" || return 1
        [ "$(grep -Ec '^TEST production 2280 RivendellDormitory [0-9]+ NoldorWarriorHorde(GilGalad)? ' "$log")" -eq 2 ] || return 1
    fi
    if covers 4400; then
        grep -E '^TEST pkg16 button frame=4400 name=Command_ConstructNoldorWarriorHordeRivendellGilGalad .* avail=0$' "$log" || return 1
        grep -E '^TEST frame 4401: produce NoldorWarriorHordeGilGalad on object [0-9]+ queued=0$' "$log" || return 1
    fi
    for frame in 4425 4465 4498; do
        if covers "$frame"; then
            [ "$(grep -Ec "^TEST horde anchor $frame [0-9]+ count [0-9]+ selected 1$" "$log")" -eq 2 ] || return 1
        fi
    done
}
