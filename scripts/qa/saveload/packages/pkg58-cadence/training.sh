check() {
    if covers 108; then
        grep -E '^TEST pkg58 frame=108 label=charge-yes .*weapon=MordorMumakilCharge .*shot=[1-9][0-9]* .*fireAllowed=1 ' "$log"
        grep -E '^TEST pkg58 frame=108 label=charge-no .*shot=0 .*fireAllowed=0 ' "$log"
    fi
    if covers 134; then
        grep -E '^TEST pkg58 frame=134 label=move-yes .*fireAllowed=1 ' "$log"
        grep -E '^TEST pkg58 frame=134 label=move-no .*fireAllowed=0 ' "$log"
    fi
    if covers 136; then
        grep -E '^TEST pkg58 frame=136 label=move-yes-shot .*shot=135 .*speed=1\.50 ' "$log"
        grep -E '^TEST pkg58 frame=136 label=move-no-shot .*shot=0 .*speed=1\.50 ' "$log"
    fi
    if covers 166; then
        grep -E '^TEST pkg58 frame=166 label=finish-yes-pre .*status=PRE_ATTACK .*committed=1 ' "$log"
        grep -E '^TEST pkg58 frame=166 label=finish-no-pre .*status=PRE_ATTACK .*committed=0 ' "$log"
    fi
    if covers 188; then
        grep -E '^TEST pkg58 frame=188 label=finish-yes-post .*shot=[1-9][0-9]* ' "$log"
        grep -E '^TEST pkg58 frame=188 label=finish-no-post .*shot=0 ' "$log"
    fi
    if covers 198; then
        grep -E '^TEST pkg58-flank frame=198 .*flanked=1$' "$log"
    fi
    if covers 235; then
        grep -E '^TEST weapon-fx frame=[0-9]+ weapon=BlackNumenoreanPolemarchSword .*flank=1$' "$log"
    fi
    if covers 275; then
        grep -E '^TEST pkg58 frame=275 label=tower-pass100 .*health=2000\.0$' "$log"
        grep -E '^TEST pkg58 frame=275 label=passenger-pass100 .*health=([0-2][0-9][0-9]|3[0-4][0-9])\.[0-9]$' "$log"
        grep -E '^TEST pkg58 frame=275 label=tower-pass0 .*health=199[0-9]\.[0-9]$' "$log"
        grep -E '^TEST pkg58 frame=275 label=passenger-pass0 .*health=350\.0$' "$log"
    fi
    if covers 340; then
        grep -E '^TEST pkg58 frame=340 label=hold-after-yes .*speed=0\.00 x=1150\.0 ' "$log"
        grep -E '^TEST pkg58 frame=340 label=hold-after-no .*speed=[1-9][0-9]*\.' "$log"
    fi
    if covers 370; then
        grep -E '^TEST projectile frame=[0-9]+ weapon=AuxillaryArcherBow .*warhead=GondorArcherBowWarhead ' "$log"
        grep -E '^TEST pkg58 frame=370 label=passenger-aux .*health=[0-9]+\.[0-9]$' "$log"
    fi
    if covers 415; then
        grep -E '^TEST pkg58 frame=415 label=hold-after-released .*speed=[1-9][0-9]*\.' "$log"
    fi
    if covers 440; then
        grep -E '^TEST pkg58 frame=440 label=aim-yes .*aiming=1 aimMask=01010 ' "$log"
        grep -E '^TEST pkg58 frame=440 label=aim-no .*aiming=0 aimMask=00000 ' "$log"
    fi
    if covers 475; then
        grep -E '^TEST pkg58 frame=475 label=hold-reload-yes .*status=RELOADING .*holdReload=1 aiming=0 aimMask=00000 .*speed=0\.00 x=1150\.0 ' "$log"
        grep -E '^TEST pkg58 frame=475 label=hold-reload-no .*status=RELOADING .*holdReload=0 .*speed=[1-9][0-9]*\.' "$log"
    fi
    if covers 602; then
        grep -E '^TEST pkg58 frame=602 label=idle-yes .*idleWait=1 ' "$log"
        grep -E '^TEST pkg58 frame=602 label=idle-no .*idleWait=0 ' "$log"
    fi
    if covers 691; then
        grep -E '^TEST pkg58 frame=691 label=instant-yes .*status=READY ' "$log"
        grep -E '^TEST pkg58 frame=691 label=instant-no .*status=RELOADING ' "$log"
    fi
}
