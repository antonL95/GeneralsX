check() {
    covers 540 || return 0
    awk '
        $1 == "TEST" && $2 == "knight-turn" && $4 == "KnightHordeTurnHorde" {
            frame = $3 + 0
            split($5, m, "="); members = m[2] + 0
            split($6, f, "="); facing = f[2] + 0
            split($7, sp, "="); speed = sp[2] + 0
            split($8, sc, "="); scalar = sc[2] + 0
            split($9, er, "="); maxErr = er[2] + 0
            split($10, yw, "="); maxYawDiff = yw[2] + 0
            split($11, ov, "="); overlaps = ov[2] + 0
            split($12, pl, "="); piles = pl[2] + 0
            if (frame >= 362 && frame <= 368 && prev_frame == frame - 1) {
                diff = facing - prev_facing
                if (diff < 0) diff = -diff
                if (diff > 3.14159265) diff = 6.2831853 - diff
                if (diff > 1.745329 && !snap_frame) {
                    snap_frame = frame
                    snap_scalar = scalar
                }
            }
            if (snap_frame && frame > snap_frame && frame <= snap_frame + 5 && maxYawDiff > 0.65) {
                if (creep_checked == 0 || speed > max_creep_speed) max_creep_speed = speed
                creep_checked = 1
            }
            if (frame == 540) {
                end_members = members
                end_err = maxErr
                end_piles = piles
            }
            prev_frame = frame
            prev_facing = facing
        }
        END {
            printf "knight-turn snap_frame=%d snap_scalar=%.4f creep_checked=%d max_creep_speed=%.4f end_members=%d end_err=%.3f end_piles=%d\n",
                snap_frame, snap_scalar, creep_checked, max_creep_speed, end_members, end_err, end_piles
            ok = (snap_frame > 0 && snap_scalar == 0 && creep_checked && max_creep_speed <= 0.40 && end_members == 10 && end_err <= 30 && end_piles == 0)
            exit !ok
        }
    ' "$log"
}
