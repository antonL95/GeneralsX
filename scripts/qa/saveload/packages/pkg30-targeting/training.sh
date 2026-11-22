check() {
	if covers 45; then
		expect "TEST pkg30 probe frame=45 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=0 damaged=1 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 script frame=45 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson ready=1"
	fi
	if covers 46; then
		expect "TEST pkg30 probe frame=46 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 cancel frame=46 button=Command_SpecialAbilityElphirBitterLesson entered=1 cleared=1"
	fi
	if covers 49; then
		expect "TEST pkg30 probe frame=49 object=DolAmrothElphir button=Command_SpecialAbilityElphirBitterLesson cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
	fi
	if covers 55; then
		expect "TEST pkg30 probe frame=55 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=1 preview=1 cursor=- ready=1"
	fi
	if covers 57; then
		expect "TEST pkg30 probe frame=57 object=IthilienAnborn button=Command_AnbornPoisonShots cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=1"
		expect "TEST pkg30 hotkey frame=57 object=IthilienAnborn button=Command_AnbornPoisonShots key=R fired=0"
		expect "TEST pkg30 script frame=57 object=IthilienAnborn button=Command_AnbornPoisonShots ready=1"
	fi
	if covers 60; then
		expect "TEST pkg30 probe frame=60 object=IthilienAnborn button=Command_AnbornPoisonShots cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=1"
	fi
	if covers 62; then
		expect "TEST pkg30 probe frame=62 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=1 preview=1 cursor=- ready=1"
	fi
	if covers 65; then
		expect "TEST pkg30 probe frame=65 object=IthilienAnborn button=Command_AnbornPoisonShots cond=1 container=1 target=0 damaged=0 valid=1 store=1 action=0 preview=0 cursor=- ready=0"
	fi
	if covers 70; then
		expect "TEST pkg30 probe frame=70 object=IthilienMablung button=Command_MablungDeadlyThrust cond=0 container=1 target=1 damaged=1 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=70 object=IthilienMablung button=Command_CreateAHero_RainOfArrows_Level1 cond=0 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=0"
		expect "TEST pkg30 script frame=70 object=IthilienMablung button=Command_MablungDeadlyThrust ready=1"
	fi
	if covers 73; then
		expect "TEST pkg30 probe frame=73 object=IthilienMablung button=Command_MablungDeadlyThrust cond=0 container=1 target=1 damaged=1 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 75; then
		expect "TEST pkg30 probe frame=75 object=IthilienMablung button=Command_MablungDeadlyThrust cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 probe frame=75 object=IthilienMablung button=Command_CreateAHero_RainOfArrows_Level1 cond=1 container=1 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=GenericInvalid ready=0"
	fi
	if covers 80; then
		expect "TEST pkg30 probe frame=80 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=80 object=Pkg30Healer button=Command_CreateAHero_SpecialAbilityAthelas cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 83; then
		expect "TEST pkg30 probe frame=83 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 85; then
		expect "TEST pkg30 probe frame=85 object=Pkg30Healer button=Pkg30CommandHealTarget cond=1 container=1 target=1 damaged=1 valid=1 store=1 action=1 preview=1 cursor=Bombard ready=1"
		expect "TEST pkg30 probe frame=85 object=Pkg30Healer button=Command_CreateAHero_SpecialAbilityAthelas cond=1 container=1 target=0 damaged=1 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
	fi
	if covers 88; then
		grep -E '^TEST pkg30 probe frame=88 object=Pkg30Healer button=Pkg30CommandHealTarget .* ready=0$' "$log"
	fi
	if covers 90; then
		expect "TEST pkg30 probe frame=90 object=Pkg30NoContainer button=Command_ExitGarrison cond=1 container=0 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=0"
		expect "TEST pkg30 probe frame=90 object=Pkg30DisabledContainer button=Command_ExitGarrison cond=1 container=0 target=0 damaged=0 valid=0 store=0 action=0 preview=0 cursor=- ready=0"
		expect "TEST pkg30 probe frame=90 object=Pkg30ValidContainer button=Command_ExitGarrison cond=1 container=1 target=0 damaged=0 valid=1 store=0 action=0 preview=0 cursor=- ready=0"
	fi
	if covers 260; then
		expect "TEST pkg30 probe frame=260 object=IthilienMablung button=Command_MablungDeadlyThrust cond=1 container=1 target=0 damaged=0 valid=0 store=1 action=0 preview=0 cursor=GenericInvalid ready=1"
		expect "TEST pkg30 probe frame=260 object=Pkg30ValidContainer button=Command_ExitGarrison cond=1 container=1 target=0 damaged=0 valid=1 store=0 action=0 preview=0 cursor=- ready=0"
	fi
}
