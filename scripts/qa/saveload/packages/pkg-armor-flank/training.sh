check() (
    if covers 456; then
        awk '/^TEST combat-coverage horde flanked-before / {before=$5; seen=1} /^TEST combat-coverage horde flanked-after / {after=$5; sampled=1} END {if (!seen || !sampled || before-after != 27) {print "Flanked armor delta:", before-after; exit 1}}' "$log"
        awk '/^TEST combat-coverage horde front-before / {before=$5; seen=1} /^TEST combat-coverage horde front-after / {after=$5; sampled=1} END {if (!seen || !sampled || before-after != 16) {print "Front armor delta:", before-after; exit 1}}' "$log"
    fi
    if covers 462; then
        awk '/^TEST health 458 PlyrCreeps MordorWitchKingOnFellBeast 0 / {for (i=1;i<=NF;i++) if ($i ~ /^health=/) h0=substr($i,8)+0} /^TEST health 460 PlyrCreeps MordorWitchKingOnFellBeast 0 / {for (i=1;i<=NF;i++) if ($i ~ /^health=/) h1=substr($i,8)+0} /^TEST health 462 PlyrCreeps MordorWitchKingOnFellBeast 0 / {for (i=1;i<=NF;i++) if ($i ~ /^health=/) h2=substr($i,8)+0} END {if (!h0 || !h1 || !h2 || h0-h1 != 75 || h2-h1 != 20) {print "WitchKing health:", h0, h1, h2; exit 1}}' "$log"
    fi
)
