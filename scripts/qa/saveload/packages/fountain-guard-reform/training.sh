check() {
    covers 810 || return 0
    awk '
        $1 == "TEST" && $2 == "fg-reform" && $4 == "FountainGuardReformHorde" {
            frame = $3 + 0
            split($5, a, "="); alt = a[2] + 0
            split($6, m, "="); members = m[2] + 0
            split($7, f, "="); facing = f[2] + 0
            split($8, sp, "="); speed = sp[2] + 0
            split($9, lt, "="); lateral = lt[2] + 0
            split($10, er, "="); maxErr = er[2] + 0
            split($11, pl, "="); piles = pl[2] + 0
            if (frame >= 271 && frame <= 280 && prev_frame == frame - 1) {
                diff = facing - prev_facing
                if (diff < 0) diff = -diff
                if (diff > 3.14159265) diff = 6.2831853 - diff
                if (diff > 0.785398 && alt == 1 && !snap_frame) {
                    snap_frame = frame
                    snap_speed = speed
                    snap_lateral = lateral
                }
            }
            if (frame == 700) {
                alt_end_active = (alt == 1 && members == 15 && speed == 0)
                alt_end_err = maxErr
                alt_end_piles = piles
            }
            if (frame == 810) {
                main_alt = alt
                main_members = members
                main_err = maxErr
                main_piles = piles
            }
            prev_frame = frame
            prev_facing = facing
        }
        $1 == "TEST" && $2 == "fg-banner" && $4 == "FountainGuardReformHorde" {
            frame = $3 + 0
            split($5, r, "="); rank = r[2] + 0
            split($6, b, "="); hasBanner = b[2] + 0
            if (frame == 810) b1 = (rank == 1 && hasBanner == 0)
        }
        END {
            printf "fg-reform snap_frame=%d snap_speed=%.4f snap_lateral=%.4f alt_err=%.3f alt_piles=%d main_alt=%d main_members=%d main_err=%.3f main_piles=%d b1=%d\n",
                snap_frame, snap_speed, snap_lateral, alt_end_err, alt_end_piles, main_alt, main_members, main_err, main_piles, b1
            ok = (snap_frame > 0 && snap_speed > 0 && snap_lateral <= 0.15 && alt_end_active && alt_end_err <= 20 && alt_end_piles == 0 && main_alt == 0 && main_members == 15 && main_err <= 35 && main_piles == 0 && b1)
            exit !ok
        }
    ' "$log"
}
