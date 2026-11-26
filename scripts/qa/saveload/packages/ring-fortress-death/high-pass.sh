check() {
    if ! grep -q ' ringchain-gollum Player_1 RingFortressGollum ' "$out/continue.txt"; then
        return 0
    fi
    if covers 1300; then
        expect 'TEST ringchain frame=1300 rings=0 holder=- upgrade=1 fortress-upgrade=1 fortress-flag=1 recruitable=1 hero=0' || return 1
    fi
    if covers 1350; then
        expect 'TEST ringchain frame=1350 rings=1 holder=- upgrade=0 fortress-upgrade=0 fortress-flag=0 recruitable=0 hero=0' || return 1
        if covers 1305; then
        awk '
            $1 == "TEST" && $2 == "all" && $3 == 1305 && $5 == "MordorFortressCitadel" { x = $6; y = $7; fortress = 1 }
            $1 == "TEST" && $2 == "all" && $3 == 1350 && $5 == "TheDroppedRing" { ring = 1; if ($6 != x || $7 != y) exit 1 }
            END { if (!fortress || !ring) exit 1 }
        ' "$log" || return 1
        fi
    fi
}
