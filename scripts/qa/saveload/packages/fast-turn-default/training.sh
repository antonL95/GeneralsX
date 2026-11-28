check() {
	if covers 246; then
		awk '
			$1 == "TEST" && $2 == "corner-speed" && $3 == 246 && $4 == "FastTurnDefaultProbe" {
				split($5, scalar, "=")
				if (scalar[2] + 0 > 0.375) fast = 1
				print
			}
			END { exit !fast }
		' "$log" || return 1
	fi
	if covers 247; then
		awk '
			$1 == "TEST" && $2 == "steering" && $3 == 246 && $4 == "FastTurnDefaultProbe" {
				facing = substr($6, 8) + 0
				hasFacing = 1
			}
			$1 == "TEST" && $2 == "steering" && $3 == 247 && $4 == "FastTurnDefaultProbe" {
				split($0, parts, "optimized:")
				split(parts[2], tail, "itinerary:")
				total = split(tail[1], points, " ")
				count = 0
				for (i = 1; i <= total; i++) {
					if (points[i] ~ /,/) {
						count++
						split(points[i], xy, ",")
						if (count == 1) { startX = xy[1] + 0; startY = xy[2] + 0 }
						if (count == 2) { dx = xy[1] - startX; dy = xy[2] - startY }
					}
				}
				if (hasFacing && count >= 3) {
					perp = dy * cos(facing) - dx * sin(facing)
					if (perp > 0) {
						radius = (dx * dx + dy * dy) / (2 * perp)
						if (radius > 18 && radius < 22) ok = 1
					}
				}
				print
			}
			END { exit !ok }
		' "$log" || return 1
	fi
}

compare() {
	for run in continue resume; do
		awk -v first="$save_frame" '$1 == "TEST" && ($2 == "corner-speed" || $2 == "steering") && $3 >= first && $4 == "FastTurnDefaultProbe"' \
			"$out/$run.log" > "$out/fast-turn-default-$run.txt"
	done
	test -s "$out/fast-turn-default-continue.txt" || return 1
	cmp "$out/fast-turn-default-continue.txt" "$out/fast-turn-default-resume.txt"
}
