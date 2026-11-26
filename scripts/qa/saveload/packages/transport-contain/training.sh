check() {
	if covers 234; then
		expect "TEST tc frame=232 action=can-contain label=unmounted valid=0"
		expect "TEST tc frame=234 action=can-contain label=mounted valid=1"
		expect "TEST tc frame=234 action=board label=mounted-board valid=1 contained=1"
	fi
	if covers 236; then
		expect "TEST tc frame=235 action=can-contain label=wagon-full valid=0"
		grep '^TEST tc frame=236 action=probe label=bone-slot1-before .* pv1=1 pv2=0 ' "$log" || return 1
		grep '^TEST tc frame=236 action=probe label=bone-slot2-before .* pv1=0 pv2=1 ' "$log" || return 1
		grep '^TEST tc frame=236 action=probe label=pips-off .* pips=0/0/0 ' "$log" || return 1
		grep '^TEST tc frame=236 action=probe label=pips-on .* pips=1/2/1 ' "$log" || return 1
		expect "TEST tc frame=236 action=manual-grab label=troll-orc manual=0 auto=1"
		expect "TEST tc frame=236 action=manual-grab label=troll-club manual=1 auto=1"
		expect "TEST tc frame=236 action=cast-grab label=troll-orc accepted=0"
	fi
	if covers 242; then
		grep '^TEST tc frame=237 action=probe label=karadd-club-state .* ws1=1 ws2=0 st1=1 st2=0 ' "$log" || return 1
		grep '^TEST tc frame=239 action=probe label=karadd-empty-state .* ws1=0 ws2=0 st1=0 st2=0 ' "$log" || return 1
		grep '^TEST tc frame=241 action=probe label=karadd-orc-before .* ws1=0 ws2=1 ' "$log" || return 1
		grep -E '^TEST tc frame=242 action=probe label=fade-enter-match-probe .* opacity=([0-9]|[1-9][0-9])$' "$log" || return 1
		grep '^TEST tc frame=242 action=probe label=fade-enter-skip-probe .* opacity=100$' "$log" || return 1
	fi
	if covers 250; then
		[ "$(grep -c '^TEST tc frame=250 action=seat label=howdah index=[0-9] bone=H4[0-9]* found=1 error=0 height=1[0-4][0-9] hidden=0 opacity=100 ' "$log")" -eq 5 ] || return 1
		grep '^TEST tc frame=250 action=seat label=karadd-seat index=0 bone=TRUNK01 found=1 error=0 ' "$log" || return 1
	fi
	if covers 252; then
		grep '^TEST tc frame=250 action=probe label=bone-slot1-after .* pv1=1 pv2=0 ' "$log" || return 1
		grep '^TEST tc frame=250 action=probe label=bone-slot2-after .* pv1=0 pv2=1 ' "$log" || return 1
		grep '^TEST tc frame=250 action=probe label=karadd-orc-after .* ws1=0 ws2=1 ' "$log" || return 1
		grep '^TEST tc frame=252 action=probe label=bone-slot1-cleared .* pv1=0 pv2=0 ' "$log" || return 1
		grep '^TEST tc frame=252 action=probe label=karadd-cleared .* ws1=0 ws2=0 ' "$log" || return 1
		grep -E '^TEST tc frame=252 action=probe label=fade-exit-match-probe .* opacity=([0-9]|[1-9][0-9])$' "$log" || return 1
	fi
	if covers 258; then
		grep '^TEST tc frame=256 action=probe label=grab-no-victim-before hp=500 ' "$log" || return 1
		grep '^TEST tc frame=256 action=probe label=grab-no-bystander-before hp=500 ' "$log" || return 1
		grep '^TEST tc frame=256 action=probe label=grab-hit-victim-before hp=500 ' "$log" || return 1
		grep '^TEST tc frame=256 action=probe label=grab-hit-bystander-before hp=500 ' "$log" || return 1
		grep '^TEST tc frame=258 action=probe label=grab-no-victim-after hp=500 unatk=1 ' "$log" || return 1
		grep '^TEST tc frame=258 action=probe label=grab-no-bystander-after hp=450 ' "$log" || return 1
		grep '^TEST tc frame=258 action=probe label=grab-hit-victim-after hp=450 unatk=0 ' "$log" || return 1
		grep '^TEST tc frame=258 action=probe label=grab-hit-bystander-after hp=450 ' "$log" || return 1
	fi
	if covers 264; then
		grep '^TEST tc frame=262 action=probe label=grab-released hp=500 unatk=0 ' "$log" || return 1
		expect "TEST tc frame=264 action=alive label=captive-destroyed present=0 alive=0"
	fi
}
