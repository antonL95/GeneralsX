check() {
	if covers 197; then
		expect 'TEST pkg29 arrow cursor frame=197 armed=1 valid=0 cursor=GenericInvalid'
		expect 'TEST pkg29 arrow frame=199 ready=1 accepted=0 firing=0 paralyzed=0'
		expect 'TEST pkg29 arrow cursor frame=200 armed=1 valid=1 cursor=Bombard'
	fi
	if covers 244; then
		accepted=1
		if [ "$phase" = resume ]; then
			accepted=0
		fi
		expect "TEST pkg29 arrow frame=244 ready=0 accepted=$accepted firing=1 paralyzed=0"
		expect 'TEST pkg29 arrow health frame=244 owner=Player_1 hp=100000.000'
	fi
	if covers 360; then
		grep -E '^TEST pkg29 arrow frame=360 ready=0 accepted=[01] firing=1 paralyzed=0$' "$log"
		expect 'TEST pkg29 arrow health frame=360 owner=Player_1 hp=100000.000'
		expect 'TEST pkg29 arrow health frame=360 owner=PlyrCreeps hp=85300.000'
	fi
	if covers 480; then
		grep -E '^TEST pkg29 arrow frame=480 ready=0 accepted=[01] firing=0 paralyzed=0$' "$log"
	fi
	if covers 511; then
		expect 'TEST pkg29 arrow frame=511 ready=0 accepted=0 firing=0 paralyzed=0'
	fi
	if covers 555; then
		expect 'TEST pkg29 arrow frame=555 ready=0 accepted=1 firing=1 paralyzed=0'
		expect 'TEST pkg29 arrow frame=560 ready=0 accepted=1 firing=0 paralyzed=1'
	fi
	if covers 690; then
		expect 'TEST pkg29 arrow frame=690 ready=0 accepted=1 firing=1 paralyzed=0'
	fi
	if covers 1080; then
		expect 'TEST pkg29 arrow cursor frame=1006 armed=1 valid=0 cursor=GenericInvalid'
		expect 'TEST pkg29 arrow frame=1008 ready=1 accepted=0 firing=0 paralyzed=0'
		expect 'TEST pkg29 arrow cursor frame=1012 armed=1 valid=1 cursor=Bombard'
		expect 'TEST pkg29 arrow frame=1080 ready=0 accepted=1 firing=1 paralyzed=0'
	fi
	if covers 1280; then
		grep -E '^TEST mods 1215 object .*\(ArnorFarm\) .* production=1\.000 ' "$log"
		expect 'TEST pkg29 arrow frame=1217 ready=1 accepted=0 firing=0 paralyzed=0'
		grep -E '^TEST mods 1280 object .*\(ArnorFarm\) .* production=3\.000 .*lists=.*MalbethPreparations' "$log"
	fi
	if covers 220; then
		expect 'TEST pkg29 poultry cursor frame=220 armed=1 valid=0 cursor=GenericInvalid'
		expect 'TEST pkg29 poultry frame=222 ready=1 accepted=0 active=0'
		expect 'TEST pkg29 poultry cursor frame=236 armed=1 valid=1 cursor=Bombard'
	fi
	if covers 244; then
		grep -E '^TEST pkg29 poultry frame=244 ready=1 accepted=[01] active=1$' "$log"
		expect 'TEST pkg29 poultry count frame=244 object=animals player=* living=0'
	fi
	if covers 290; then
		expect 'TEST pkg29 poultry count frame=290 object=animals player=* living=1'
		expect 'TEST pkg29 poultry count frame=290 object=Pkg29PoultryTarget player=PlyrCreeps living=0'
		expect 'TEST pkg29 poultry count frame=290 object=Pkg29PoultryTarget player=Player_1 living=1'
		grep -E '^TEST pkg29 poultry object frame=290 name=Chicken_Replacement neutral=1 .* hp=1\.0/1\.0 xp=0\.0 upgrades=0$' "$log"
		grep -E '^TEST pkg29 poultry frame=295 ready=0 accepted=[01] active=0$' "$log"
	fi
	if covers 580; then
		expect 'TEST pkg29 poultry count frame=580 object=animals player=* living=0'
		expect 'TEST pkg29 poultry count frame=580 object=Pkg29PoultryTarget player=PlyrCreeps living=0'
	fi
	if covers 670; then
		expect 'TEST pkg29 poultry frame=636 ready=1 accepted=1 active=1'
		expect 'TEST pkg29 poultry count frame=670 object=Pkg29PoultryTarget player=PlyrCreeps living=0'
		grep -E '^TEST pkg29 poultry count frame=670 object=animals player=\* living=[01]$' "$log"
		expect 'TEST pkg29 poultry frame=675 ready=0 accepted=1 active=0'
	fi
	if covers 1000; then
		expect 'TEST pkg29 poultry frame=965 ready=1 accepted=1 active=1'
		expect 'TEST pkg29 poultry count frame=1000 object=Pkg29PoultryTarget player=PlyrCreeps living=0'
	fi
	if covers 225; then
		expect 'TEST pkg29 swoop object frame=225 valid=0 accepted=0'
		expect 'TEST pkg29 swoop location frame=227 valid=0 accepted=0'
		expect 'TEST pkg29 swoop object frame=230 valid=1 accepted=1'
		expect 'TEST pkg29 swoop state frame=234 active=1 attacking=0 victim=0 hp=100000.0'
	fi
	if covers 244; then
		expect 'TEST pkg29 swoop state frame=244 active=1 attacking=1 victim=1 hp=100000.0'
	fi
	if covers 260; then
		expect 'TEST pkg29 swoop state frame=260 active=1 attacking=1 victim=1 hp=100000.0'
		expect 'TEST pkg29 swoop object frame=262 valid=0 accepted=0'
	fi
	if covers 420; then
		grep -E '^TEST pkg29 swoop state frame=420 active=1 attacking=1 victim=1 hp=[1-9][0-9]{0,4}\.0$' "$log"
	fi
	if covers 480; then
		expect 'TEST pkg29 swoop location frame=455 valid=1 accepted=1'
		expect 'TEST pkg29 swoop state frame=457 active=1 attacking=0 victim=0 hp=100000.0'
		grep -E '^TEST pkg29 swoop state frame=480 active=0 attacking=[01] victim=0 hp=100000\.0$' "$log"
	fi
	if covers 555; then
		expect 'TEST pkg29 swoop script frame=520 accepted=0'
		expect 'TEST pkg29 swoop script frame=535 accepted=1'
		grep -E '^TEST pkg29 swoop state frame=555 active=1 attacking=1 victim=1 hp=[0-9]+\.0$' "$log"
	fi
}
