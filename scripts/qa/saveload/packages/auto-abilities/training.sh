check() {
	if covers 219; then
		expect 'TEST auto state frame=219 power=SpecialPowerShieldBubbleGondor ready=1 next=210 dead=0 disabled=0 bubble=0'
		expect 'TEST auto state frame=219 power=AutoAbilitiesFastShield ready=1 next=210 dead=0 disabled=0 bubble=0'
		test "$(grep -Fc 'TEST auto slot frame=219 entry=7 button=Command_GondorShieldBubble ring=0 side=0 radial=0' "$log")" -eq 2
		expect 'TEST auto slot frame=219 entry=4 button=Command_ConstructGondorFighterHorde ring=1 side=0 radial=1'
		expect 'TEST auto slot frame=219 entry=7 button=Command_PurchaseUpgradeGondorBarracksLevel2 ring=0 side=1 radial=1'
	fi
	if covers 221; then
		expect 'TEST auto hit frame=220 damage=10.0'
		expect 'TEST auto state frame=221 power=SpecialPowerShieldBubbleGondor ready=0 next=2470 dead=0 disabled=0 bubble=1'
		expect 'TEST auto state frame=221 power=AutoAbilitiesFastShield ready=0 next=370 dead=0 disabled=0 bubble=0'
	fi
	for frame in 244 246; do
		if covers "$frame"; then
			expect "TEST auto state frame=$frame power=AutoAbilitiesFastShield ready=0 next=370 dead=0 disabled=0 bubble=0"
		fi
	done
	if covers 369; then
		expect 'TEST auto hit frame=369 damage=10.0'
		expect 'TEST auto state frame=369 power=AutoAbilitiesFastShield ready=0 next=370 dead=0 disabled=0 bubble=0'
	fi
	if covers 371; then
		expect 'TEST auto state frame=371 power=AutoAbilitiesFastShield ready=0 next=520 dead=0 disabled=0 bubble=0'
	fi
	if covers 520; then
		expect 'TEST auto state frame=520 power=AutoAbilitiesFastShield ready=0 next=520 dead=0 disabled=1 bubble=0'
	fi
	if covers 530; then
		expect 'TEST auto state frame=530 power=AutoAbilitiesFastShield ready=1 next=525 dead=1 disabled=0 bubble=0'
	fi
}
