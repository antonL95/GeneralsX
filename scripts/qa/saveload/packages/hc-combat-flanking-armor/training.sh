check() (
    if covers 237; then
        expect 'TEST fireweapon-cadence health 237 flanking-dot-first 80'
    fi
    if covers 271; then
        expect 'TEST fireweapon-cadence health 271 flanking-delay 80'
        expect 'TEST fireweapon-cadence health 271 flanking-dot 60'
    fi
    if covers 316; then
        awk '/^TEST combat-coverage horde flank-before / {before=$5; seen=1} /^TEST combat-coverage horde flank-after / {after=$5; sampled=1} END {if (!seen || !sampled || before-after != 20) {print "Flank health delta:", before-after; exit 1}}' "$log"
    fi
    if covers 439; then
        awk '/^TEST combat-coverage cash bounty-before / {before=$5; seen=1} /^TEST combat-coverage cash bounty-after / {after=$5; sampled=1} END {if (!seen || !sampled || after-before != 50) {print "Cash bounty delta:", after-before; exit 1}}' "$log"
        awk '/^TEST combat-coverage cash bounty-after / {before=$5; seen=1} /^TEST combat-coverage cash structure-bounty / {after=$5; sampled=1} END {if (!seen || !sampled || after-before != 20) {print "Structure bounty delta:", after-before; exit 1}}' "$log"
        expect 'TEST fireweapon-cadence health 409 override-hit 593'
        awk '/^TEST fireweapon-cadence health 406 heal-damaged / {damage=$6; seen=1} /^TEST fireweapon-cadence health 439 heal-pulsed / {pulse=$6; sampled=1} END {if (!seen || !sampled || pulse-damage != 15) {print "Heal delta:", pulse-damage; exit 1}}' "$log"
    fi
)
