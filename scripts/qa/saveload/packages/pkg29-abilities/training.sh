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
}
