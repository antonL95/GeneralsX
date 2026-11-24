check() {
    if covers 311; then
        expect "TEST corner-cell 311 same=1 neighbor=0" || return 1
    fi
    if covers 231; then
        awk '
            $1 == "TEST" && $2 == "corner-speed" && $3 == 231 && $4 == "CornerRoundingKnock" {
                split($5, scalar, "=")
                split($6, pair, "=")
                split(pair[2], velocity, ",")
                if (scalar[2] == 0 && velocity[1] * velocity[1] + velocity[2] * velocity[2] > 0.33334 * 0.33334) found = 1
                print
            }
            END { exit !found }
        ' "$log" || return 1
    fi
    if covers 250; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 250 && $4 == "GondorFountainGuardHorde" {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                count = 0
                curved = 0
                for (i = 1; i <= total; i++) {
                    if (points[i] ~ /,/) {
                        count++
                        split(points[i], xy, ",")
                        if (xy[1] > 2850 && xy[1] < 3050 && (xy[1] % 5 != 0 || xy[2] % 5 != 0)) curved = 1
                    }
                }
                hordes++
                if (hordes == 1 && count >= 4) found = 1
                if (hordes == 2 && count >= 4 && curved) corner = 1
                for (i = 1; i <= NF; i++) {
                    if ($i ~ /^planar=/) {
                        split($i, value, "=")
                        split(value[2], velocity, ",")
                        speedSquared = velocity[1] * velocity[1] + velocity[2] * velocity[2]
                        if (hordes == 1 && speedSquared > 0.41667 * 0.41667) fast = 1
                        if (hordes == 3 && speedSquared < 0.41667 * 0.41667) slow = 1
                    }
                }
                print
            }
            $1 == "TEST" && $2 == "corner-speed" && $3 == 250 && $4 == "GondorFountainGuardHorde" {
                split($5, scalar, "=")
                scalarHordes++
                if (scalarHordes == 1 && scalar[2] > 0.41667) fastScalar = 1
                if (scalarHordes == 2 && scalar[2] < 0.41667) slowScalar = 1
                print
            }
            $1 == "TEST" && $2 == "steering" && $3 == 250 && $4 == "WildAzog" {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                azogs++
                for (i = 1; i <= total; i++) {
                    if (points[i] ~ /,/) {
                        split(points[i], xy, ",")
                        if (xy[1] % 5 != 0 || xy[2] % 5 != 0) {
                            if (azogs == 1 && xy[2] < 1399) retry = 1
                            if (azogs == 2 && xy[2] > 1701) normal = 1
                        }
                    }
                }
                print
            }
            $1 == "TEST" && $2 == "steering" && $3 == 250 && ($4 == "CornerRoundingSlow" || $4 == "CornerRoundingKnock") {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                count = 0
                initialArc = 0
                for (i = 1; i <= total; i++) {
                    if (points[i] ~ /,/) {
                        count++
                        split(points[i], xy, ",")
                        if (count == 1) { startX = xy[1]; startY = xy[2] }
                        if (count == 2) { dx = xy[1] - startX; dy = xy[2] - startY }
                        if (count > 1 && (xy[1] % 5 != 0 || xy[2] % 5 != 0) &&
                            (xy[1] - startX) * (xy[1] - startX) + (xy[2] - startY) * (xy[2] - startY) < 6400)
                            initialArc = 1
                    }
                }
                if ($4 == "CornerRoundingKnock" && count >= 2 && !initialArc) knocked = 1
                if ($4 == "CornerRoundingSlow" && count >= 3 && dy > 0) {
                    radius = (dx * dx + dy * dy) / (2 * dy)
                    if (radius > 16 && radius < 24) slowRadius = 1
                }
                print
            }
            END { exit !(found && corner && fast && slow && fastScalar && slowScalar && retry && normal && knocked && slowRadius) }
        ' "$log" || return 1
    fi
    if covers 260; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 260 && $4 == "GondorFountainGuardHorde" {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                count = 0
                for (i = 1; i <= total; i++) if (points[i] ~ /,/) count++
                if (count >= 4) rounded++
                print
            }
            $1 == "TEST" && $2 == "corner-speed" && $3 == 260 && $4 == "CornerRoundingRest" {
                split($5, scalar, "=")
                if (scalar[2] > 0.33334) moving = 1
                print
            }
            END { exit !(rounded == 2 && moving) }
        ' "$log" || return 1
    fi
    if covers 269; then
        awk '
            $1 == "TEST" && $2 == "corner-speed" && $3 == 269 && ($4 == "RohanTreeBerd" || $4 == "CornerRoundingTreeControl") {
                split($5, scalar, "=")
                if (scalar[2] > 0.33334) scalarFast[$4] = 1
                print
            }
            $1 == "TEST" && $2 == "steering" && $3 == 269 && ($4 == "RohanTreeBerd" || $4 == "CornerRoundingTreeControl") {
                for (i = 1; i <= NF; i++) {
                    if ($i ~ /^pos=/) {
                        split($i, value, "=")
                        split(value[2], pos, ",")
                    }
                    if ($i ~ /^facing=/) {
                        split($i, value, "=")
                        facing = value[2]
                    }
                    if ($i ~ /^planar=/) {
                        split($i, value, "=")
                        split(value[2], velocity, ",")
                        speedSquared = velocity[1] * velocity[1] + velocity[2] * velocity[2]
                    }
                }
                targetY = $4 == "RohanTreeBerd" ? 550 : 670
                turn = atan2(targetY - pos[2], 2400 - pos[1]) - facing
                while (turn > 3.14159265) turn -= 6.2831853
                while (turn < -3.14159265) turn += 6.2831853
                if (turn < 0) turn = -turn
                if (speedSquared > 0.33334 * 0.33334 && turn > 2.0944 && turn < 3.14159) found[$4] = 1
                print
            }
            END { exit !(found["RohanTreeBerd"] && found["CornerRoundingTreeControl"] &&
                scalarFast["RohanTreeBerd"] && scalarFast["CornerRoundingTreeControl"]) }
        ' "$log" || return 1
    fi
    if covers 280; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 280 && ($4 == "RohanTreeBerd" || $4 == "CornerRoundingTreeControl") {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                count = 0
                initialArc = 0
                gridWaypoints = 0
                for (i = 1; i <= total; i++) {
                    if (points[i] ~ /,/) {
                        count++
                        split(points[i], xy, ",")
                        if (count == 1) {
                            startX = xy[1]
                            startY = xy[2]
                        } else {
                            if (xy[1] % 5 == 0 && xy[2] % 5 == 0) gridWaypoints++
                            if ((xy[1] % 5 != 0 || xy[2] % 5 != 0) &&
                                (xy[1] - startX) * (xy[1] - startX) + (xy[2] - startY) * (xy[2] - startY) < 6400) {
                                initialArc = 1
                            }
                        }
                    }
                }
                if ($4 == "RohanTreeBerd" && count == 2 && xy[1] > 2390 && xy[1] < 2410 &&
                    xy[2] > 540 && xy[2] < 560) capped = 1
                if ($4 == "CornerRoundingTreeControl" && count >= 4 && initialArc && gridWaypoints == 1 &&
                    xy[1] > 2390 && xy[1] < 2410 && xy[2] > 660 && xy[2] < 680) control = 1
                print
            }
            END { exit !(capped && control) }
        ' "$log" || return 1
    fi
    if covers 311; then
        awk '
            $1 == "TEST" && $2 == "corner-speed" && $3 == 311 && $4 == "CornerRoundingRest" {
                split($5, scalar, "=")
                split($6, pair, "=")
                split(pair[2], velocity, ",")
                if (scalar[2] == 0 && velocity[1] * velocity[1] + velocity[2] * velocity[2] > 0.33334 * 0.33334) found = 1
                print
            }
            END { exit !found }
        ' "$log" || return 1
    fi
    if covers 320; then
        awk '
            $1 == "TEST" && $2 == "steering" && $3 == 320 && $4 == "CornerRoundingRest" {
                split($0, parts, "optimized:")
                split(parts[2], tail, "itinerary:")
                total = split(tail[1], points, " ")
                count = 0
                initialArc = 0
                for (i = 1; i <= total; i++) {
                    if (points[i] ~ /,/) {
                        count++
                        split(points[i], xy, ",")
                        if (count == 1) { startX = xy[1]; startY = xy[2] }
                        if (count > 1 && (xy[1] % 5 != 0 || xy[2] % 5 != 0) &&
                            (xy[1] - startX) * (xy[1] - startX) + (xy[2] - startY) * (xy[2] - startY) < 6400)
                            initialArc = 1
                    }
                }
                if (count >= 2 && !initialArc) found = 1
                print
            }
            END { exit !found }
        ' "$log" || return 1
    fi
}

compare() {
    for object in GondorFountainGuardHorde RohanTreeBerd CornerRoundingTreeControl CornerRoundingSlow CornerRoundingKnock CornerRoundingRest; do
        awk -v object="$object" '$1 == "TEST" && $2 == "steering" && $3 == 330 && $4 == object { sub(/^.* optimized:/, "optimized:"); sub(/ itinerary:.*$/, ""); print }' "$out/continue.log" > "$out/corner-$object-continue.txt"
        awk -v object="$object" '$1 == "TEST" && $2 == "steering" && $3 == 330 && $4 == object { sub(/^.* optimized:/, "optimized:"); sub(/ itinerary:.*$/, ""); print }' "$out/resume.log" > "$out/corner-$object-resume.txt"
        test -s "$out/corner-$object-continue.txt" || return 1
        expected=1
        if [ "$object" = GondorFountainGuardHorde ]; then expected=3; fi
        awk -v expected="$expected" '{ count = 0; for (i = 2; i <= NF; i++) if ($i ~ /,/) count++; if (count < 2) exit 1 } END { if (NR != expected) exit 1 }' "$out/corner-$object-continue.txt" || return 1
        cmp "$out/corner-$object-continue.txt" "$out/corner-$object-resume.txt" || return 1
    done
    if [ -n "${CORNER_ROUNDING_LEGACY_DIR:-}" ]; then
        sh "$package_dir/legacy.sh" "$CORNER_ROUNDING_LEGACY_DIR" "$out" "$PKGS1_BUILD_DIR" || return 1
    else
        printf '%s\n' 'Legacy save check skipped: set CORNER_ROUNDING_LEGACY_DIR.'
    fi
}
