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
				if ($5 == "hidden=1" && $6 == "weapon_hidden=1" && $7 == "has_weapon=0") ok = 1
			}
			END { exit !ok }
		' "$log" || return 1
	fi
	if covers 1120; then
		awk '
			$1 == "TEST" && $2 == "toggle-hidden" {
				print
				if ($4 == "object=AngmarNaglur") {
					if ($3 == "frame=245" || $3 == "frame=600" || $3 == "frame=1120") {
						if ($5 == "hidden=1" && $6 == "weapon_hidden=1" && $7 == "has_weapon=0") hiddenCount++
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
				if ($3 == "frame=1155" && $4 == "object=AngmarNaglur" &&
					$5 == "hidden=0" && $6 == "weapon_hidden=0" && $7 == "has_weapon=1")
					unhid = 1
				if ($4 == "object=ToggleHiddenTarget") {
					split($8, h, "=")
					hp[$3] = h[2] + 0
				}
			}
			END {
				resumed = ("frame=1120" in hp) && ("frame=1230" in hp) && hp["frame=1230"] < hp["frame=1120"]
				exit !(unhid && resumed)
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
