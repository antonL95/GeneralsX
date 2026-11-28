prepare() {
	for frame in 233 255 258; do
		printf '%s corner-speed Player_1 AccDecDefaultProbe 0\n' "$frame"
		printf '%s steering-path Player_1 AccDecDefaultProbe 0\n' "$frame"
	done
	frame=295
	while [ "$frame" -le 330 ]; do
		printf '%s corner-speed Player_1 AccDecDefaultProbe 0\n' "$frame"
		printf '%s steering-path Player_1 AccDecDefaultProbe 0\n' "$frame"
		frame=$((frame + 1))
	done
}

check() {
	if covers 258; then
		awk '
			$1 == "TEST" && $2 == "steering" && $4 == "AccDecDefaultProbe" && ($3 == 255 || $3 == 258) {
				split($15, a, "=")
				accel[$3] = a[2] + 0
				print
			}
			END {
				exit !(accel[255] == 1 && accel[258] == 0)
			}
		' "$log" || return 1
	fi
	if covers 330; then
		awk '
			$1 == "TEST" && $2 == "corner-speed" && $4 == "AccDecDefaultProbe" {
				split($5, value, "=")
				scalar[$3] = value[2] + 0
			}
			$1 == "TEST" && $2 == "steering" && $4 == "AccDecDefaultProbe" && $3 > 296 {
				split($16, value, "=")
				decel[$3] = value[2] + 0
				print
			}
			END {
				for (frame = 297; frame <= 330; frame++) {
					if (!(frame in scalar)) continue
					if (!(frame in decel)) failed = 1
					if (scalar[frame] >= 1.01 && decel[frame] != 0) failed = 1
					if (scalar[frame] > 0 && scalar[frame] < 0.99) {
						if (decel[frame] != 1) failed = 1
						on = 1
					}
					if (on && scalar[frame] == 0) {
						if (decel[frame] != 0) failed = 1
						off = 1
					}
				}
				exit !(on && off && !failed)
			}
		' "$log" || return 1
	fi
}

compare() {
	for run in continue resume; do
		awk -v first="$save_frame" '
			$1 == "TEST" && ($2 == "corner-speed" || $2 == "steering") && $3 + 0 >= first && $4 == "AccDecDefaultProbe"
		' "$out/$run.log" > "$out/accdectrigger-default-$run.txt"
	done
	test -s "$out/accdectrigger-default-continue.txt" || return 1
	cmp "$out/accdectrigger-default-continue.txt" "$out/accdectrigger-default-resume.txt"
}
