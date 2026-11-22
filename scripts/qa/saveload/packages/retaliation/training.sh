check() {
    if covers 254; then
        horde=$(sed -n 's/^TEST frame 235: create PartitionScaleHorde for Player_1 id=\([0-9]*\) .*/\1/p' "$out/continue.log")
        bulwark=$(sed -n 's/^TEST frame 230: create RetaliationSaveBulwark for PlyrCreeps id=\([0-9]*\) .*/\1/p' "$out/continue.log")
        test -n "$horde"
        test -n "$bulwark"
        grep -E "^TEST damage frame=254 weapon=FortressArrowTowerBowWarhead .*source=$bulwark victim=[0-9]+\(RetaliationSaveCottage\) .*dealt=30\.00 " "$log"
        expect "TEST horde retaliate 254 $horde $bulwark"
    fi
}
