check() {
    if covers 244; then
        grep -E '^TEST save-load partition frame=244 template=(GondorCastleWallSegment|GondorCastleWallTrebuchet|MenWallGate|PartitionUpgradeBuilding|PartitionScaleFighter|PartitionScaleHorde) .* count=[1-9][0-9]*$' "$log" > "$out/$phase.cells"
        test "$(wc -l < "$out/$phase.cells")" -eq 6
        grep -E 'template=MenWallGate .* count=12$' "$out/$phase.cells"
        grep -E 'template=PartitionUpgradeBuilding major=100\.00 ' "$out/$phase.cells"
        grep -E 'template=PartitionScaleFighter major=96\.00 ' "$out/$phase.cells"
        grep -E 'template=PartitionScaleHorde major=120\.00 ' "$out/$phase.cells"
    fi
    if covers 450; then
        grep -E '^TEST damage frame=450 weapon=MordorCatapultRockWarhead .*victim=[0-9]+\(MenWallGate\) .*dealt=4\.00 ' "$log"
        grep -E '^TEST health 450 Player_1 MenWallGate 0 id=[0-9]+ health=1495\.99 ' "$log"
    fi
}

compare() {
    if [ "$save_frame" -le 244 ] && covers 244; then
        cmp "$out/continue.cells" "$out/resume.cells"
    fi
}
