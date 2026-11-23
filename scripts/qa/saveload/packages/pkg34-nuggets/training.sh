check() {
	if covers 242; then
		grep -E '^TEST pkg34 frame=242 label=victim-after .* stun=1 model=1 .* freeze=1 hit=2(3[3-9]|4[0-2]) weapon=Pkg34FilteredStrike .* fx=1$' "$log" || return 1
		awk '
			/label=(source|victim)-(before|after) / { split($5, field, "="); hp[$4] = field[2] + 0 }
			END {
				healed = hp["label=source-after"] - hp["label=source-before"]
				dealt = hp["label=victim-before"] - hp["label=victim-after"]
				if (length(hp) != 4 || dealt != 20 || healed != dealt / 2)
					exit 1
				printf "pkg34 damage=%d drained=%d\n", dealt, healed
			}
		' "$log" || return 1
	fi
	if covers 246; then
		grep '^TEST pkg34 frame=246 label=victim-resumed .* stun=1 model=1 .* freeze=1 .* fx=1$' "$log" || return 1
	fi
	if covers 253; then
		grep '^TEST pkg34 frame=253 label=control-after .* stun=0 model=0 .* fx=0$' "$log" || return 1
	fi
	if covers 311; then
		grep '^TEST pkg34 frame=311 label=victim-expired .* stun=0 model=0 ' "$log" || return 1
	fi
	if covers 244; then
		grep '^TEST pkg34 frame=244 label=shock-center .* model=1 .* impact=0 ' "$log" || return 1
		grep '^TEST pkg34 frame=244 label=shock-early .* impact=0 ' "$log" || return 1
	fi
	if covers 270; then
		grep '^TEST pkg34 frame=270 label=shock-near-arrived .* impact=1 ' "$log" || return 1
	fi
	if covers 276; then
		grep '^TEST pkg34 frame=276 label=shock-far-waiting .* impact=0 ' "$log" || return 1
	fi
	if covers 286; then
		grep '^TEST pkg34 frame=286 label=shock-far-arrived .* impact=1 ' "$log" || return 1
	fi
	if covers 354; then
		grep '^TEST pkg34 frame=352 label=gate-before .* gate=0 ' "$log" || return 1
		grep '^TEST pkg34 frame=354 label=gate-after .* stun=1 .* gate=1 .* freeze=0 ' "$log" || return 1
	fi
	if covers 357; then
		awk '
			/label=money-/ { split($(NF - 1), field, "="); cash[$4] = field[2] + 0 }
			END {
				if (length(cash) != 4 || cash["label=money-victim-before"] <= 0 ||
				    cash["label=money-victim-after"] != 0 ||
				    cash["label=money-source-after"] - cash["label=money-source-before"] != cash["label=money-victim-before"])
					exit 1
				printf "pkg34 cash moved=%d victim=%d\n", cash["label=money-source-after"] - cash["label=money-source-before"], cash["label=money-victim-after"]
			}
		' "$log" || return 1
	fi
	if covers 392; then
		grep '^TEST pkg34 frame=392 label=lua-victim .* fear=1 ' "$log" || return 1
		grep '^TEST pkg34 frame=392 label=lua-ally .* fear=0 ' "$log" || return 1
	fi
	if covers 470; then
		grep '^TEST pkg34 frame=396 label=terror-same-side .* terror=0 ' "$log" || return 1
		grep '^TEST pkg34 frame=402 label=terror-victim .* terror=1 ' "$log" || return 1
		grep '^TEST pkg34 frame=470 label=terror-expired .* terror=0 ' "$log" || return 1
	fi
	if covers 477; then
		grep '^TEST pkg34 frame=475 label=grab-control .* parent=0 ' "$log" || return 1
		grep -E '^TEST pkg34 frame=477 label=grab-victim .* parent=[1-9][0-9]* ' "$log" || return 1
	fi
}
