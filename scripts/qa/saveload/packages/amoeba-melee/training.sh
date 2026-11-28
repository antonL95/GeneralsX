check() {
    covers 350 || return 0
    has_init=0
    if covers 215; then
        has_init=1
    fi
    awk -v has_init="$has_init" '
        $1 == "TEST" && $2 == "amoeba-probe" && $4 == "AmoebaMeleeAttackerHorde" {
            frame = $3 + 0
            split($5, e, "="); eng = e[2] + 0
            split($6, f, "="); facing = f[2] + 0
            split($7, t, "="); targetAngle = t[2] + 0
            split($8, a, "="); angleErr = a[2] + 0
            split($9, l, "="); living = l[2] + 0
            split($10, k, "="); attacking = k[2] + 0
            split($11, d, "="); maxDist = d[2] + 0
            split($12, s, "="); slots = s[2]
            if (frame == 215) {
                init_slots = slots
                init_err = angleErr
            }
            if (eng == 1 && !first_eng_frame) {
                first_eng_frame = frame
                first_eng_err = angleErr
                first_eng_slots = slots
            }
            if (eng == 1) {
                eng_samples++
                if (angleErr > max_eng_err) max_eng_err = angleErr
                if (maxDist > max_eng_dist) max_eng_dist = maxDist
                if (attacking > max_attacking) max_attacking = attacking
            }
        }
        END {
            reformed = (init_slots != "" && first_eng_slots != "" && init_slots != first_eng_slots) ? 1 : 0
            printf "amoeba-melee init_err=%.4f first_eng=%d first_eng_err=%.4f max_eng_err=%.4f reformed=%d max_attacking=%d max_eng_dist=%.2f samples=%d\n",
                init_err, first_eng_frame, first_eng_err, max_eng_err, reformed, max_attacking, max_eng_dist, eng_samples
            ok = (first_eng_frame > 0 && first_eng_err <= 0.05 && max_eng_err <= 0.05 && max_attacking > 0 && max_eng_dist <= 95.0 && eng_samples >= 4)
            if (has_init)
                ok = ok && (init_err > 1.0 && reformed == 1)
            exit !ok
        }
    ' "$log"
}
