check_lines() {
    for line do
        frame=${line#*frame=}
        frame=${frame%% *}
        if covers "$frame"; then
            expect "$line" || return 1
        fi
    done
}

check_pattern() {
    if covers "$1" && ! grep -E "$2" "$log"; then
        printf 'Missing in %s at frame %s: %s\n' "$phase" "$1" "$2" >&2
        return 1
    fi
}

check() {
    check_lines \
        'PKG35 taint frame=227 label=Player_1:IsengardTaintLand:0 at=3500,125 match=1' \
        'PKG35 taint frame=227 label=- at=3500,400 match=1' \
        'PKG35 taint frame=236 label=Player_1:IsengardTaintLand:0 at=3310,125 match=1' \
        'PKG35 taint frame=236 label=- at=3360,125 match=1' \
        'PKG35 taint frame=236 label=Player_1:IsengardTaintLand:1 at=3500,125 match=1' \
        'PKG35 taint frame=244 label=Player_1:IsengardTaintLand:1 at=3500,125 match=1' \
        'PKG35 taint frame=249 label=- at=3310,125 match=1' \
        'PKG35 taint frame=249 label=Player_1:IsengardTaintLand:0 at=3500,125 match=1' \
        'PKG35 taint frame=259 label=- at=3500,125 match=1' || return 1

    if ! grep -Fq 'pkg35-cast Player_1 MordorSpellBook' "$out/commands.txt"; then
        return 0
    fi
    check_lines \
        'TEST weather frame=216 state=1 end=5615' \
        'PKG35 count frame=230 player=Player_1 type=DarknessCaster count=1' \
        'TEST weather frame=283 state=1 end=5615' \
        'PKG35 count frame=310 player=Player_1 type=DarknessCaster count=0' \
        'PKG35 count frame=310 player=Player_1 type=FreezingRainCaster count=1' \
        'PKG35 count frame=310 player=Player_1 type=FreezingRainWeatherTrigger count=1' \
        'TEST weather frame=421 state=4 end=1320' \
        'PKG35 count frame=430 player=Player_1 type=FreezingRainCaster count=0' \
        'PKG35 count frame=430 player=neutral type=RainFX count=0' \
        'PKG35 count frame=525 player=Player_1 type=ElessarOathbreakerFlood count=1' \
        'PKG35 count frame=525 player=Player_1 type=FloodKotD count=1' \
        'PKG35 count frame=701 player=Player_1 type=PartTheHeavens count=1' \
        'PKG35 count frame=763 player=Player_1 type=PartTheHeavens count=1' \
        'PKG35 count frame=763 player=Player_1 type=Pkg35EnvironmentControl count=1' \
        'PKG35 count frame=850 player=Player_1 type=ElessarOathbreakerFlood count=0' \
        'PKG35 count frame=860 player=Player_1 type=PartTheHeavens count=0' \
        'TEST weather frame=1320 state=0 end=0' \
        'PKG35 count frame=1321 player=Player_1 type=DarknessCaster count=0' \
        'PKG35 count frame=1355 player=Player_1 type=WhiteWinterCaster count=1' \
        'PKG35 count frame=1355 player=Player_1 type=WhiteWinterWeatherTrigger count=1' \
        'TEST weather frame=1431 state=4 end=2030' \
        'PKG35 count frame=1440 player=Player_1 type=WhiteWinterCaster count=0' \
        'PKG35 count frame=1440 player=neutral type=WhiteWinterFX count=0' \
        'TEST weather frame=2030 state=0 end=0' \
        'PKG35 count frame=2060 player=Player_1 type=DarknessCaster count=1' \
        'PKG35 count frame=2080 player=Player_1 type=DarknessCaster count=0' \
        'TEST weather frame=2965 state=0 end=0' \
        'PKG35 count frame=2966 player=Player_1 type=DarknessCaster count=0' \
        'TEST weather frame=2976 state=0 end=0' \
        'TEST weather frame=4791 state=0 end=0' \
        'PKG35 count frame=4791 player=Player_1 type=SunflareSunbeam count=1' || return 1

    check_pattern 310 '^PKG35 count frame=310 player=neutral type=RainFX count=[1-9][0-9]*$' || return 1
    check_pattern 1355 '^PKG35 count frame=1355 player=neutral type=WhiteWinterFX count=[1-9][0-9]*$' || return 1
    check_pattern 288 '^TEST terrain fire frame=288 .* rate=[1-9][0-9]* .* x=2605 y=455$' || return 1
    check_pattern 310 '^TEST terrain fire frame=310 .* rate=0 .* x=2605 y=455$' || return 1
    for frame in 236 244 251 1391 4774 4781 4791; do
        check_pattern "$frame" "^PKG35 mods frame=$frame .* list=SpellBookMarchToWar active=1 " || return 1
    done
    for frame in 310 1431 4775 4783; do
        check_pattern "$frame" "^PKG35 mods frame=$frame .* list=SpellBookMarchToWar active=0 " || return 1
    done
    check_pattern 365 '^PKG35 mods frame=365 .* list=FreezingRainDebuff active=1 damage=0.750$' || return 1
    check_pattern 515 '^PKG35 mods frame=515 .* list=FreezingRainDebuff active=0 damage=1.000$' || return 1
    check_lines \
        'PKG35 mods frame=231 ordinal=0 list=Pkg35Debuff active=1 damage=0.500' \
        'PKG35 mods frame=310 ordinal=0 list=Pkg35Debuff active=1 damage=0.500' \
        'PKG35 mods frame=310 ordinal=0 list=Pkg35BlockedAttempt active=0 damage=0.500' \
        'PKG35 mods frame=1431 ordinal=0 list=Pkg35Debuff active=1 damage=0.500' \
        'PKG35 mods frame=2976 ordinal=0 list=SpellBookMarchToWar active=1 damage=1.200' \
        'PKG35 mods frame=2976 ordinal=0 list=Pkg35Debuff active=1 damage=1.200' \
        'PKG35 mods frame=4775 ordinal=0 list=Pkg35Debuff active=1 damage=0.500' \
        'PKG35 mods frame=4781 ordinal=0 list=SpellBookMarchToWar active=1 damage=0.600'
}
