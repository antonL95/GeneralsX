check() {
	:
}

compare() {
	phase=continue log="$out/continue.log" expect "TEST bcaravan census frame=220 template=GuldurPorter living=3"
	phase=continue log="$out/continue.log" expect "TEST bcaravan census frame=236 template=GuldurPorter living=2"
	for run in continue resume; do
		phase=$run log="$out/$run.log" expect "TEST bcaravan census frame=900 template=GuldurPorter living=1"
		phase=$run log="$out/$run.log" expect "TEST bcaravan census frame=900 template=GuldurSlaveMill living=1"
		phase=$run log="$out/$run.log" expect "TEST bcaravan owned frame=900 upgrade=Upgrade_AngmarDarkIronArmor owned=1"
		phase=$run log="$out/$run.log" expect "TEST bcaravan owned frame=900 upgrade=Upgrade_AngmarDarkIronArmor owned=0"
		phase=$run log="$out/$run.log" expect "TEST upgrade_button 780 Command_PurchaseUpgradeAngmarIceArrows Upgrade_AngmarIceArrows reason=available frames=300"
	done
}
