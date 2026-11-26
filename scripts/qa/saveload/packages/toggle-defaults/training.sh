check() {
    resource='complete=1 set=RohanFarmCommandSet user7=0 money=[0-9]* Upgrade_BuildArmyPikes=0 Upgrade_BuildArmySwords=0'
    unit='complete=1 set=RohanFarmCommandSet_Peasantry user7=0 money=[0-9]* Upgrade_BuildArmyPikes=1 Upgrade_BuildArmySwords=0'
    for frame in 700 899; do
        if covers "$frame"; then
            grep -q "TOGGLE state frame=$frame actor=RohanFarm $resource" "$log" || { echo "FAIL farm not in resource mode frame=$frame"; return 1; }
        fi
    done
    for frame in 970 1100; do
        if covers "$frame"; then
            grep -q "TOGGLE state frame=$frame actor=RohanFarm $unit" "$log" || { echo "FAIL farm not in unit mode frame=$frame"; return 1; }
        fi
    done
}
