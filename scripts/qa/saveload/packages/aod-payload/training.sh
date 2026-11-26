check_lines() {
    for line do
        frame=${line#*frame=}
        frame=${frame%% *}
        if covers "$frame"; then
            expect "$line" || return 1
        fi
    done
}

check() {
    check_lines \
        'TEST aod-payload horde frame=220 object=RohanOathbreakerHordeSmall count=16 max=16' \
        'TEST aod-payload count frame=220 object=RohanOathbreaker count=16 contained=16' \
        'TEST aod-payload horde frame=244 object=RohanOathbreakerHordeSmall count=16 max=16' \
        'TEST aod-payload count frame=244 object=RohanOathbreaker count=16 contained=16' \
        'TEST aod-payload horde frame=260 object=RohanOathbreakerHordeSmall count=16 max=16' \
        'TEST aod-payload count frame=260 object=RohanOathbreaker count=16 contained=16'
}
