check() {
	if covers 190; then
		grep '^TEST pkg31-container frame=190 label=first-boarded count=1 cap=2 ' "$log" || return 1
		grep '^TEST tc frame=190 action=probe label=first-weapon-state .* st1=1 st2=0 ' "$log" || return 1
		grep '^TEST tc frame=190 action=probe label=visible-on-board .* opacity=100$' "$log" || return 1
	fi
	if covers 242; then
		grep '^TEST pkg31-container frame=242 label=both-boarded count=2 cap=2 ' "$log" || return 1
		grep '^TEST tc frame=242 action=probe label=second-weapon-state .* st1=0 st2=1 ' "$log" || return 1
	fi
	if covers 250; then
		grep '^TEST pkg31-container frame=250 label=both-boarded-resumed count=2 cap=2 ' "$log" || return 1
		grep '^TEST tc frame=250 action=probe label=second-weapon-state-resumed .* st1=0 st2=1 ' "$log" || return 1
	fi
	if covers 265; then
		expect 'TEST pkg31-exit-button frame=251 button=Command_ExitGarrison'
		grep '^TEST pkg31-container frame=265 label=rider-remains count=1 cap=2 ' "$log" || return 1
		grep '^TEST tc frame=265 action=probe label=cleared-on-exit .* st1=0 st2=0 ' "$log" || return 1
		grep '^TEST tc frame=265 action=probe label=visible-on-exit .* opacity=100$' "$log" || return 1
	fi
}
