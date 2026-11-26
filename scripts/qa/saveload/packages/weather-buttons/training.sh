check() {
    grep -Fq 'weather-buttons-state' "$out/commands.txt" || return 0
    for line in \
        'WEATHERBUTTON frame=25 slot=1 unlocked=0 valid=0' \
        'WEATHERBUTTON frame=25 slot=2 unlocked=0 valid=0' \
        'WEATHERBUTTON frame=35 slot=1 unlocked=1 valid=1 ready=1 percent=1.000 internal=0 accepted=0' \
        'WEATHERBUTTON frame=35 slot=2 unlocked=1 valid=1 ready=1 percent=1.000 internal=0 accepted=0' \
        'WEATHERBUTTON frame=140 slot=1 unlocked=1 valid=0 ready=0' \
        'WEATHERBUTTON frame=300 slot=2 unlocked=1 valid=0 ready=0' \
        'WEATHERBUTTON frame=5700 slot=1 unlocked=1 valid=0 ready=0' \
        'WEATHERBUTTON frame=5700 slot=2 unlocked=1 valid=0 ready=0' \
        'WEATHERBUTTON frame=11200 slot=1 unlocked=1 valid=1 ready=1 percent=1.000 internal=0 accepted=0' \
        'WEATHERBUTTON frame=11200 slot=2 unlocked=1 valid=1 ready=1 percent=1.000 internal=0 accepted=0'
    do
        frame=${line#*frame=}
        frame=${frame%% *}
        if covers "$frame"; then
            grep -F "$line" "$log" || return 1
        fi
    done
    for frame in 140 300; do
        if covers "$frame"; then
            grep -E "WEATHERBUTTON frame=$frame .* internal=1 accepted=1" "$log" || return 1
        fi
    done
    if covers 5700; then
        expect 'TEST weather frame=5700 state=0 end=0' || return 1
        for slot in 1 2; do
            grep -E "WEATHERBUTTON frame=5700 slot=$slot .* percent=0\.[1-9][0-9]* internal=0 accepted=0" "$log" || return 1
        done
    fi
}
