check() {
	if covers 1365; then
		expect "TEST pkg30 probe frame=1365 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=0 damaged=1 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 script frame=1365 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson ready=1"
	fi
	if covers 1366; then
		expect "TEST pkg30 probe frame=1366 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 cancel frame=1366 button=Command_SpecialAbilityElphirBitterLesson entered=1 cleared=1"
	fi
	if covers 1369; then
		expect "TEST pkg30 probe frame=1369 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
	fi
	if covers 1375; then
		expect "TEST pkg30 probe frame=1375 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=1 preview=1 cursor=- ready=1"
	fi
	if covers 1377; then
		expect "TEST pkg30 probe frame=1377 object=IthilienAnborn button=Command_AnbornPoisonShots cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=1"
		expect "TEST pkg30 hotkey frame=1377 object=IthilienAnborn button=Command_AnbornPoisonShots key=R fired=0"
		expect "TEST pkg30 script frame=1377 object=IthilienAnborn button=Command_AnbornPoisonShots ready=1"
	fi
	if covers 1380; then
		expect "TEST pkg30 probe frame=1380 object=IthilienAnborn button=Command_AnbornPoisonShots cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=1"
	fi
	if covers 1382; then
		expect "TEST pkg30 probe frame=1382 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=1 preview=1 cursor=- ready=1"
	fi
	if covers 1385; then
		expect "TEST pkg30 probe frame=1385 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=0 preview=0 cursor=- ready=0"
	fi
	if covers 1390; then
		expect "TEST pkg30 probe frame=1390 object=IthilienMablung button=Command_MablungDeadlyThrust cond=0 container=1 target=1 damaged=1 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=1390 object=IthilienMablung button=Command_CreateAHero_RainOfArrows_Level1 cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=0"
		expect "TEST pkg30 script frame=1390 object=IthilienMablung button=Command_MablungDeadlyThrust ready=1"
	fi
	if covers 1393; then
		expect "TEST pkg30 probe frame=1393 object=IthilienMablung button=Command_MablungDeadlyThrust cond=0 container=1 target=1 damaged=1 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 1395; then
		expect "TEST pkg30 probe frame=1395 object=IthilienMablung button=Command_MablungDeadlyThrust cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 probe frame=1395 object=IthilienMablung button=Command_CreateAHero_RainOfArrows_Level1 cond=1 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=0"
	fi
	if covers 1400; then
		expect "TEST pkg30 probe frame=1400 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=1400 object=Pkg30Healer button=Command_CreateAHero_SpecialAbilityAthelas cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 1403; then
		expect "TEST pkg30 probe frame=1403 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 1405; then
		expect "TEST pkg30 probe frame=1405 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 probe frame=1405 object=Pkg30Healer button=Command_CreateAHero_SpecialAbilityAthelas cond=1 container=1 target=0 damaged=1 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 1408; then
		grep -E '^TEST pkg30 probe frame=1408 object=Pkg30Healer button=Pkg30CommandHealTarget .* ready=0$' "$log"
	fi
	if covers 1410; then
		expect "TEST pkg30 probe frame=1410 object=Pkg30NoContainer button=Command_ExitGarrison cond=1 container=0 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=0"
		expect "TEST pkg30 probe frame=1410 object=Pkg30DisabledContainer button=Command_ExitGarrison cond=1 container=0 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=0"
		expect "TEST pkg30 probe frame=1410 object=Pkg30ValidContainer button=Command_ExitGarrison cond=1 container=1 target=0 damaged=0 valid=1 store=0 action=0 preview=0 cursor=- ready=0"
	fi
	if covers 1440; then
		expect "TEST pkg30 probe frame=1440 object=IthilienMablung button=Command_MablungDeadlyThrust cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=1440 object=Pkg30ValidContainer button=Command_ExitGarrison cond=1 container=1 target=0 damaged=0 valid=1 store=0 action=0 preview=0 cursor=- ready=0"
	fi
}
