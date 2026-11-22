check() {
	if covers 204; then
		expect 'TEST pkg29 cursor frame=204 valid=0 cursor=GenericInvalid generic=0'
		expect 'TEST pkg29 cursor frame=206 valid=0 cursor=GenericInvalid generic=0'
		expect 'TEST pkg29 cursor frame=207 valid=0 cursor=GenericInvalid generic=0'
		expect 'TEST pkg29 repair frame=210 module=1 ready=1 accepted=0 task=0 repairing=0 health=1970.000'
		expect 'TEST pkg29 cursor frame=211 valid=1 cursor=DoRepair generic=0'
		grep '^TEST pkg29 repair frame=214 module=1 ready=1 accepted=1 task=1 ' "$log"
	fi
	if covers 244; then
		grep -E '^TEST pkg29 repair frame=244 .* task=1 repairing=1 ' "$log"
	fi
	if covers 247; then
		expect 'TEST pkg29 cursor frame=247 valid=0 cursor=GenericInvalid generic=0'
	fi
	if covers 320; then
		grep -E '^TEST pkg29 repair frame=320 module=1 ready=1 .* task=0 repairing=0 health=2000.000$' "$log"
	fi
	if covers 332; then
		grep '^TEST pkg29 repair frame=332 module=1 ready=1 accepted=1 task=1 ' "$log"
	fi
	if covers 427; then
		grep '^TEST pkg29 repair frame=427 module=1 ready=1 accepted=1 task=1 ' "$log"
	fi
	if covers 521; then
		expect 'TEST pkg29 cursor frame=521 valid=1 cursor=DoRepair generic=0'
	fi
	if covers 522; then
		expect 'TEST pkg29 cursor frame=522 valid=1 cursor=DoRepair generic=0'
	fi
	if covers 526; then
		grep '^TEST pkg29 repair frame=526 module=1 ready=1 accepted=1 task=1 ' "$log"
	fi
	if covers 527; then
		grep '^TEST pkg29 repair frame=527 module=1 ready=1 accepted=1 ' "$log"
	fi
	if covers 800; then
		grep -E '^TEST pkg29 repair frame=800 .* health=2000.000$' "$log"
	fi
	if covers 801; then
		grep -E '^TEST pkg29 repair frame=801 .* health=2000.000$' "$log"
	fi
}
