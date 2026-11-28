prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 220; frame <= 304 && frame <= end; ++frame)
            print frame, "bezier-ground-hit-state Player_1 BezierGroundHitBody 0"
        for (frame = 335; frame <= 435 && frame <= end; ++frame)
            print frame, "bezier-ground-hit-state Player_1 BezierGroundHitGimli 0"
    }'
}

check() {
    if covers 305; then
        awk '
            /^TEST bezier-ground-hit state / {
                for (i = 3; i <= NF; ++i) {
                    split($i, kv, "=")
                    f[kv[1]] = kv[2]
                }
                if (f["name"] == "BezierGroundHitBody") {
                    split(f["pos"], p, ",")
                    x = p[1] + 0; y = p[2] + 0; z = p[3] + 0
                    fl = f["flight"] + 0
                    dead = f["dead"] + 0
                    splat = f["splatted"] + 0
                    if (fl == 1 && prev_fl == 1 && prev_z > z + 0.5)
                        descending = 1
                    if (fl == 1 && prev_fl == 1 && descending && z > prev_z + 0.5) {
                        bounces++
                        descending = 0
                        bounce_x[bounces] = prev_x
                        bounce_y[bounces] = prev_y
                    }
                    if (prev_fl == 1 && fl == 0 && !landed) {
                        landed = 1
                        land_x = x
                        land_y = y
                        land_dead = dead
                        land_splatted = splat
                    }
                    prev_fl = fl
                    prev_x = x; prev_y = y; prev_z = z
                }
            }
            END {
                printf "thrown bounces=%d b1=(%.2f,%.2f) b2=(%.2f,%.2f) final=(%.2f,%.2f) dead=%d splatted=%d\n",
                    bounces, bounce_x[1], bounce_y[1], bounce_x[2], bounce_y[2], land_x, land_y, land_dead, land_splatted
                ok = (bounces == 2) &&
                    bounce_x[1] >= 1710 && bounce_x[1] <= 1730 && bounce_y[1] >= 1670 && bounce_y[1] <= 1690 &&
                    bounce_x[2] >= 1750 && bounce_x[2] <= 1770 && bounce_y[2] >= 1675 && bounce_y[2] <= 1695 &&
                    land_x >= 1790 && land_x <= 1810 && land_y >= 1680 && land_y <= 1705 &&
                    land_dead == 1 && land_splatted == 1
                exit !ok
            }
        ' "$log"
        grep -E '^TEST damage frame=[0-9]+ weapon=BezierGroundHitPrimaryWarhead .*dealt=110\.00 health=5000\.00->4890\.00$' "$log"
        test "$(grep -Ec '^TEST damage frame=[0-9]+ weapon=BezierGroundHitBounceWarhead .*dealt=45\.00 health=5000\.00->4955\.00$' "$log")" -eq 2
    fi
    if covers 475; then
        grep -E '^TEST damage frame=[0-9]+ weapon=RohanGimliLeapWarhead .*dealt=304\.00 health=5000\.00->4696\.00$' "$log"
        awk '
            /^TEST bezier-ground-hit state / {
                for (i = 3; i <= NF; ++i) {
                    split($i, kv, "=")
                    f[kv[1]] = kv[2]
                }
                if (f["name"] == "BezierGroundHitGimli") {
                    fr = f["frame"] + 0
                    split(f["pos"], p, ",")
                    if (fr == 386) {
                        busy_land = f["busy"] + 0
                        land_x = p[1] + 0
                    }
                    if (fr == 405) {
                        busy_stuck = f["busy"] + 0
                        stuck_x = p[1] + 0
                    }
                    if (fr == 475) {
                        busy_done = f["busy"] + 0
                        done_x = p[1] + 0
                    }
                }
            }
            END {
                printf "gimli land=(busy=%d,x=%.2f) stuck=(busy=%d,x=%.2f) released=(busy=%d,x=%.2f)\n",
                    busy_land, land_x, busy_stuck, stuck_x, busy_done, done_x
                exit !(busy_land == 1 && land_x >= 1969 && busy_stuck == 1 && stuck_x >= 1969 && busy_done == 0 && done_x < 1955)
            }
        ' "$log"
    fi
}

compare() {
    for phase in continue resume; do
        awk -v save="$save_frame" '/^TEST bezier-ground-hit state / { split($4, fr, "="); if (fr[2] + 0 > save) print }' \
            "$out/$phase.log" > "$out/bezier-ground-hit-$phase.trace"
    done
    diff -u "$out/bezier-ground-hit-continue.trace" "$out/bezier-ground-hit-resume.trace"
}
