check() {
	if covers 235; then
		awk '
			$1 == "TEST" && $2 == "toggle-hidden" && $3 == "frame=235" {
				print
				if ($4 == "object=AngmarNaglur" && $5 == "hidden=0" && $6 == "weapon_hidden=0" && $7 == "has_weapon=1") hero = 1
				if ($4 == "object=ToggleHiddenTarget") {
					split($8, h, "=")
					if (h[2] + 0 < 50000) damaged = 1
				}
			}
			END { exit !(hero && damaged) }
		' "$log" || return 1
	fi
	if covers 243; then
		awk '
			$1 == "TEST" && $2 == "toggle-hidden" && $3 == "frame=243" && $4 == "object=AngmarNaglur" {
				print
				split($9, r, "=")
				split($10, rf, "=")
				if ($5 == "hidden=1" && $6 == "weapon_hidden=1" && $7 == "has_weapon=0" &&
					r[2] == 0 && rf[2] - 243 == 2700 && $11 == "percent=0.0000") ok = 1
			}
			END { exit !ok }
		' "$log" || return 1
	fi
	if covers 1120; then
		awk '
			$1 == "TEST" && $2 == "toggle-hidden" {
				print
				if ($4 == "object=AngmarNaglur") {
					split($3, f, "=")
					split($9, r, "=")
					split($10, rf, "=")
					if ($3 == "frame=245" || $3 == "frame=600" || $3 == "frame=1120") {
						if ($5 == "hidden=1" && $6 == "weapon_hidden=1" && $7 == "has_weapon=0" &&
							r[2] == 0 && rf[2] - (f[2] + 0) == 2700 && $11 == "percent=0.0000") hiddenCount++
					}
				}
				if ($4 == "object=ToggleHiddenTarget") {
					split($8, h, "=")
					hp[$3] = h[2] + 0
				}
			}
			END {
				noShot = ("frame=245" in hp) && hp["frame=600"] == hp["frame=245"] && hp["frame=1120"] == hp["frame=245"]
				if ("frame=243" in hp)
					noShot = noShot && hp["frame=245"] == hp["frame=243"]
				exit !(hiddenCount == 3 && noShot)
			}
		' "$log" || return 1
	fi
	if covers 1230; then
		awk '
			$1 == "TEST" && $2 == "toggle-hidden" {
				print
				if ($4 == "object=AngmarNaglur") {
					split($9, r, "=")
					split($10, rf, "=")
					split($11, p, "=")
					if ($3 == "frame=1155" && $5 == "hidden=0" && $6 == "weapon_hidden=0" && $7 == "has_weapon=1") {
						unhid = 1
						rf1155 = rf[2] + 0
						p1155 = p[2] + 0
					}
					if ($3 == "frame=1230" && $5 == "hidden=0" && $6 == "weapon_hidden=0" && $7 == "has_weapon=1" && r[2] == 0) {
						rf1230 = rf[2] + 0
						p1230 = p[2] + 0
					}
				}
				if ($4 == "object=ToggleHiddenTarget") {
					split($8, h, "=")
					hp[$3] = h[2] + 0
				}
			}
			END {
				resumed = ("frame=1120" in hp) && ("frame=1230" in hp) && hp["frame=1230"] < hp["frame=1120"]
				countdown = rf1155 > 2940 && rf1230 == rf1155 && p1155 > 0 && p1230 > p1155
				exit !(unhid && resumed && countdown)
			}
		' "$log" || return 1
	fi
}

compare() {
	for run in continue resume; do
		awk -v first="$save_frame" '$1 == "TEST" && $2 == "toggle-hidden" {
			split($3, f, "="); if (f[2] + 0 >= first) print
		}' "$out/$run.log" > "$out/toggle-hidden-$run.txt"
	done
	test -s "$out/toggle-hidden-continue.txt" || return 1
	cmp "$out/toggle-hidden-continue.txt" "$out/toggle-hidden-resume.txt"
}
