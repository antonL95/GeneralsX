check() {
    if covers 225; then
        expect 'TEST frame 225: power SpecialAbilityFortressHornOfErebor button Command_HornsoftheGate'
    fi
    if covers 235; then
        expect 'TEST frame 235: power SpecialAbilityFortressHornOfErebor button Command_SpecialAbilityHornOfTauron_ForPalantir'
    fi
    if covers 244; then
        expect 'HORN frame=244 player=Player_1 ordinal=0 list=BoromirGondorHornAntiCategory active=1 remaining=140 mask=2'
        expect 'HORN frame=244 player=PlyrCivilian ordinal=0 list=BoromirGondorHornAntiCategory active=1 remaining=140 mask=2'
        expect 'HORN frame=244 player=PlyrCreeps ordinal=0 list=BoromirGondorHornAntiCategory active=0 remaining=0 mask=0'
        expect 'HORN frame=244 player=PlyrCreeps ordinal=1 list=BoromirGondorHornAntiCategory active=0 remaining=0 mask=0'
    fi
    if covers 255; then
        expect 'HORN frame=255 player=Player_1 ordinal=1 list=HornOfTauronDebuff active=0 remaining=0 mask=0'
        expect 'HORN frame=255 player=PlyrCivilian ordinal=1 list=HornOfTauronDebuff active=0 remaining=0 mask=0'
        expect 'HORN frame=255 player=PlyrCreeps ordinal=2 list=HornOfTauronDebuff active=1 remaining=589 mask=0'
        expect 'HORN frame=255 player=PlyrCreeps ordinal=3 list=HornOfTauronDebuff active=0 remaining=0 mask=0'
    fi
    if covers 260; then
        grep -E '^TEST ability 260 Player_1 DwarvenFortressCitadel .* cond=PACKING$' "$log" || return 1
        grep -E '^TEST ability 260 Player_1 MirkwoodCitadel .* cond=PACKING$' "$log" || return 1
    fi
    if covers 400; then
        expect 'HORN frame=400 player=Player_1 ordinal=0 list=BoromirGondorHornAntiCategory active=0 remaining=0 mask=0'
        expect 'HORN frame=400 player=PlyrCivilian ordinal=0 list=BoromirGondorHornAntiCategory active=0 remaining=0 mask=0'
    fi
    if covers 850; then
        expect 'HORN frame=850 player=PlyrCreeps ordinal=2 list=HornOfTauronDebuff active=0 remaining=0 mask=0'
    fi
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '$1 == "HORN" && $2 ~ /^frame=/ { split($2, f, "="); if (f[2] >= first) print }' "$out/$phase.log" > "$out/horn-$phase.txt"
    done
    diff -u "$out/horn-continue.txt" "$out/horn-resume.txt"
}
