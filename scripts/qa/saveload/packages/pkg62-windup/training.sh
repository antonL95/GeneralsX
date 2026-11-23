check() {
    if covers 244; then
        grep -E '^TEST pkg62 actor frame=244 name=Pkg62IceActor attack=1 firing=0 preattack=1 ' "$log"
    fi
    if covers 275; then
        grep -E '^TEST pkg62 actor frame=275 name=Pkg62IceActor attack=0 firing=0 preattack=1 ' "$log"
        grep -E '^TEST pkg62 actor frame=275 name=Pkg62FollowActor attack=0 firing=0 preattack=0 ' "$log"
        grep -E '^TEST pkg62 slot frame=275 name=Pkg62IceActor slot=0 active=0 weapon=AngmarStoneThrowerIceRock pre=0 shot=258 next=498 ' "$log"
        grep -E '^TEST pkg62 slot frame=275 name=Pkg62IceActor slot=1 active=1 weapon=AngmarStoneThrowerRock pre=0 shot=0 next=498 ' "$log"
        grep -E '^TEST pkg62 slot frame=275 name=Pkg62TimerActor slot=1 active=1 weapon=AngmarStoneThrowerRock pre=0 shot=0 next=230 ' "$log"
    fi
    if covers 350; then
        grep -E '^TEST pkg62 actor frame=348 name=Pkg62IceActor attack=0 firing=0 preattack=1 ' "$log"
        grep -E '^TEST pkg62 actor frame=350 name=Pkg62IceActor attack=0 firing=0 preattack=0 ' "$log"
    fi
    if covers 454; then
        grep -E "^TEST pkg62 actor frame=454 name=HaradKaradd .* self=1 " "$log"
        grep -E "^TEST pkg62 actor frame=454 name=Pkg62LeapControlActor .* self=0 " "$log"
    fi
    if covers 480; then
        grep -E "^TEST pkg62 actor frame=480 name=HaradKaradd .* self=0 .* pos=160\.00,2980\.00," "$log"
    fi
    if covers 560; then
        grep -E '^TEST pkg62 slot frame=556 name=Pkg62RandomActor slot=0 active=1 weapon=Pkg62RandomOnly pre=(0 shot=555|(55[6-9]|5[6-9][0-9]|60[0-9]|61[0-5]) shot=0) ' "$log"
        grep -E '^TEST pkg62 slot frame=560 name=Pkg62RandomControlActor slot=0 active=1 weapon=Pkg62RandomControl pre=0 shot=555 ' "$log"
    fi
    if covers 656; then
        corsair=$(sed -n 's/^TEST pkg62 actor frame=648 name=Pkg62CorsairActor .* id=//p' "$log")
        control=$(sed -n 's/^TEST pkg62 actor frame=648 name=Pkg62CorsairControlActor .* id=//p' "$log")
        expect "RETAIL AUDIO start frame=650 owner=$corsair sound=CorsairVoiceAttackFirebomb"
        expect "RETAIL AUDIO start frame=654 owner=$control sound=CorsairVoiceAttack"
        grep -Eq "^RETAIL AUDIO start frame=[0-9]+ owner=$corsair sound=CorsairVoiceEnterStateAttackFirebomb$" "$log"
        grep -Eq "^RETAIL AUDIO start frame=[0-9]+ owner=$control sound=CorsairVoiceEnterStateAttack$" "$log"
    fi
    if covers 744; then
        caster=$(sed -n 's/^TEST pkg62 actor frame=660 name=Pkg62FxActor .* id=//p' "$log")
        control=$(sed -n 's/^TEST pkg62 actor frame=660 name=Pkg62FxControlActor .* id=//p' "$log")
        grep -Eq "^\[Retail FXList\] Sound GandalfLightningSummon owner=$caster action=start frame=" "$log"
        ! grep -Fq "Sound GandalfLightningSummon owner=$control " "$log"
        timings=$(awk '/^TEST pkg62 slot frame=744 name=Pkg62Fx/ { print $10, $11 }' "$log" | sort -u)
        test "$timings" = 'shot=735 next=885'
    fi
}

compare() {
    for phase in continue resume; do
        awk -v save="$save_frame" '/^TEST pkg62 / { split($4, frame, "="); if (frame[2] + 0 > save) print }' \
            "$out/$phase.log" > "$out/pkg62-$phase.trace"
    done
    diff -u "$out/pkg62-continue.trace" "$out/pkg62-resume.trace"
}
