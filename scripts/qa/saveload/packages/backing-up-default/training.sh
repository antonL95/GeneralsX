check() {
	for frame in 240 245 255; do
		if covers "$frame"; then
			awk -v frame="$frame" '
				$1 == "TEST" && $2 == "pkg69" && $3 == frame && $4 == "BackingUpDefaultProbe" {
					print
					ok = $6 == 1 && $7 == 1 && $8 == 1 && $10 < -0.90 &&
						$12 > 0.99 && $15 == 1 && $16 == 0.75
				}
				END { exit !ok }
			' "$log" || return 1
		fi
	done
}

compare() {
	for run in continue resume; do
		awk -v first="$save_frame" '
			$1 == "TEST" && $2 == "pkg69" && $3 + 0 >= first && $4 == "BackingUpDefaultProbe"
		' "$out/$run.log" > "$out/backing-up-default-$run.txt"
	done
	test -s "$out/backing-up-default-continue.txt" || return 1
	cmp "$out/backing-up-default-continue.txt" "$out/backing-up-default-resume.txt"
}
