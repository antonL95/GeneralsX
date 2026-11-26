actor() {
    printf 'WEATHERNUGGET actor frame=%s player=%s list=WhiteWinterDebuff active=%s dealt=%s taken=%s shroud=%s' "$@"
}

check() {
    grep -Fq 'weathernugget-actor' "$out/commands.txt" || return 0
    for line in \
        "$(actor 38 PlyrCreeps 0 1.000 1.000 25.0)" \
        'TEST terrain fire frame=39 burning=0 fuel=70 rate=20 health=1.0 recovering=0 x=2605 y=455' \
        'TEST weather frame=140 state=1 end=5441' \
        'PKG35 count frame=140 player=Player_1 type=WhiteWinterCaster count=1' \
        'PKG35 count frame=140 player=Player_1 type=WhiteWinterWeatherTrigger count=1' \
        'PKG35 count frame=140 player=neutral type=WhiteWinterFX count=90' \
        "$(actor 140 Player_1 0 1.000 1.000 25.4)" \
        "$(actor 140 PlyrCreeps 1 0.750 1.250 17.5)" \
        'TEST terrain fire frame=140 burning=0 fuel=51 rate=0 health=1.0 recovering=0 x=2605 y=455' \
        "$(actor 1100 Player_1 0 1.000 1.000 25.4)" \
        "$(actor 1100 PlyrCreeps 1 0.750 1.250 17.5)" \
        'TEST weather frame=5400 state=1 end=5441' \
        'PKG35 count frame=5400 player=neutral type=WhiteWinterFX count=90' \
        "$(actor 5400 PlyrCreeps 1 0.750 1.250 17.5)" \
        'TEST weather frame=5600 state=0 end=0' \
        'PKG35 count frame=5600 player=Player_1 type=WhiteWinterCaster count=0' \
        'PKG35 count frame=5600 player=neutral type=WhiteWinterFX count=0' \
        "$(actor 5600 PlyrCreeps 0 1.000 1.000 25.0)" \
        'TEST terrain fire frame=5690 burning=25 fuel=32 rate=20 health=1.0 recovering=0 x=2805 y=455'
    do
        frame=${line#*frame=}
        frame=${frame%% *}
        if covers "$frame"; then
            expect "$line" || return 1
        fi
    done
}
