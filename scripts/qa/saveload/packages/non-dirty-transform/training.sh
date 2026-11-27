prepare() {
    awk -v end="$end" 'BEGIN {
        for (frame = 180; frame <= 375 && frame <= end; ++frame) {
            print frame, "non-dirty-transform-trace Player_1 NonDirtyRouteBird 0"
            print frame, "non-dirty-transform-trace Player_1 DirtyRouteBird 0"
        }
    }'
}

check_bird() {
    local bird="$1" expect_dirty="$2"
    awk -v bird="$bird" -v expect_dirty="$expect_dirty" -v save_frame="$save_frame" '
        function abs(v) { return v < 0 ? -v : v }
        /^TEST non-dirty-transform / {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                sample[field[1]] = field[2]
            }
            if (sample["bird"] != bird)
                next
            frame = sample["frame"] + 0
            x = sample["x"] + 0
            y = sample["y"] + 0
            z = sample["z"] + 0
            ix = sample["ix"] + 0
            iy = sample["iy"] + 0
            dx = sample["dx"] + 0
            dy = sample["dy"] + 0
            dz = sample["dz"] + 0
            if (!seen) {
                seen = 1
                start_y = y
                start_ix = ix
                start_iy = iy
            }
            if (y > max_y)
                max_y = y
            if (ix != start_ix || iy != start_iy)
                trigger_moved = 1
            if (abs(dx - x) > 0.05 || abs(dy - y) > 0.05 || abs(dz - z) > 0.05)
                drawable_mismatch = 1
            if (frame == save_frame && y > 1420)
                moved_at_save = 1
            if (frame > save_frame + 40 && y > max_y_after)
                max_y_after = y
            last_x = x
            last_y = y
            last_ix = ix
            last_iy = iy
            non_dirty = sample["non_dirty"] + 0
        }
        END {
            printf "%s seen=%d dy=%.1f moved_at_save=%d trigger_moved=%d drawable_mismatch=%d non_dirty=%d start_i=(%d,%d) last_i=(%d,%d) last=(%.1f,%.1f)\n",
                bird, seen, max_y - start_y, moved_at_save, trigger_moved, drawable_mismatch, non_dirty,
                start_ix, start_iy, last_ix, last_iy, last_x, last_y
            ok = seen && (max_y - start_y) > 200 && moved_at_save && !drawable_mismatch
            if (expect_dirty)
                ok = ok && !non_dirty && trigger_moved && abs(last_iy - int(last_y)) <= 2
            else
                ok = ok && non_dirty && !trigger_moved
            exit !ok
        }
    ' "$log"
}

check() {
    check_bird NonDirtyRouteBird 0 || return 1
    check_bird DirtyRouteBird 1 || return 1
}
