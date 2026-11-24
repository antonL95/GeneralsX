check() {
	if covers 239; then
		expect 'TEST cah profile frame=239 valid=1'
		grep -E '^TEST cah state frame=239 template=CreateAHero profile=1 appearance=1 button=1 rank=7 xp=[0-9]+\.5 ready=[01] selected=1$' "$log"
	fi
	if covers 243; then
		before=$(sed -n 's/^TEST cah state frame=239 .* xp=\([0-9.]*\) .*/\1/p' "$log")
		expect "TEST cah state frame=243 template=CreateAHeroScaled profile=1 appearance=1 button=1 rank=7 xp=$before ready=1 selected=1"
	fi
	if covers 247; then
		grep -E '^TEST cah state frame=247 template=CreateAHeroScaled profile=1 appearance=1 button=1 rank=7 xp=[0-9]+\.5 ready=1 selected=[01]$' "$log"
	fi
	if covers 251; then
		expect 'PW rank frame=248 object=CreateAHeroScaled rank=10'
		expect 'TEST cah cast frame=250 valid=1 disabled=0'
		grep -E '^TEST cah state frame=251 template=CreateAHeroScaled profile=1 appearance=1 button=1 rank=10 xp=[0-9]+\.0 ready=[01] selected=1$' "$log"
	fi
	if covers 257; then
		grep -E '^TEST timed weapon frame=257 source=[0-9]+\(DreadPlagueDamager1\) weapon=DreadPlagueWeapon1 ' "$log"
	fi
	if covers 260; then
		grep -E '^TEST frame 260: destroy object [0-9]+ \(DreadPlague1\)$' "$log"
	fi
}

compare() {
	before=$(sed -n 's/^TEST cah state frame=239 .* xp=\([0-9.]*\) .*/\1/p' "$out/continue.log")
	for branch in continue resume; do
		grep -E "^TEST cah state frame=247 template=CreateAHeroScaled .* rank=7 xp=$before ready=1 selected=[01]$" "$out/$branch.log"
	done
}
