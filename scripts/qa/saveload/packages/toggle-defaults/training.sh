check() {
    resource='complete=1 set=RohanFarmCommandSet user7=1 money=[0-9]* xp=[0-9]* Upgrade_BuildArmyPikes=0 Upgrade_BuildArmySwords=1'
    unit='complete=1 set=RohanFarmCommandSet_Peasantry user7=0 money=[0-9]* xp=[0-9]* Upgrade_BuildArmyPikes=1 Upgrade_BuildArmySwords=0'
    for frame in 700 899 1190; do
        if covers "$frame"; then
            grep -q "TOGGLE state frame=$frame actor=RohanFarm $resource" "$log" || { echo "FAIL farm not in resource mode frame=$frame"; return 1; }
        fi
    done
    for frame in 970 1100; do
        if covers "$frame"; then
            grep -q "TOGGLE state frame=$frame actor=RohanFarm $unit" "$log" || { echo "FAIL farm not in unit mode frame=$frame"; return 1; }
        fi
    done
    if covers 700 && covers 899; then
        before=$(sed -n 's/.*TOGGLE state frame=700 actor=RohanFarm .* xp=\([0-9]*\) .*/\1/p' "$log" | head -1)
        after=$(sed -n 's/.*TOGGLE state frame=899 actor=RohanFarm .* xp=\([0-9]*\) .*/\1/p' "$log" | head -1)
        [ "$after" -gt "$before" ] || { echo "FAIL farm earned nothing itself in resource mode"; return 1; }
    fi
}
