check() {
	for frame in 240 245 255; do
		if covers "$frame"; then
			awk -v frame="$frame" '
				$1 == "TEST" && $2 == "pkg69" && $3 == frame {
					print
					if ($4 == "CanMoveBackwardsFourProbe")
						four = $6 == 1 && $7 == 1 && $8 == 1 && $10 < -0.90 && $12 > 0.99
					if ($4 == "CanMoveBackwardsNoProbe")
						no = $6 == 0 && $7 == 0 && $9 == 1
					if ($4 == "CanMoveBackwardsOneProbe")
						one = $8 == 1
				}
				END { exit !(four && no && one) }
			' "$log" || return 1
		fi
	done
}

compare() {
	for run in continue resume; do
		awk -v first="$save_frame" '
			$1 == "TEST" && $2 == "pkg69" && $3 + 0 >= first && $4 ~ /^CanMoveBackwards(Four|No|One)Probe$/
		' "$out/$run.log" > "$out/can-move-backwards-int-$run.txt"
	done
	test -s "$out/can-move-backwards-int-continue.txt" || return 1
	cmp "$out/can-move-backwards-int-continue.txt" "$out/can-move-backwards-int-resume.txt"
}
