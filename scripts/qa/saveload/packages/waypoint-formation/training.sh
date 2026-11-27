prepare() {
    grep -q 'create Player_1 WaitFormationNear' "$out/commands.txt" || return 0
    awk -v end="$end" 'BEGIN {
        for (frame = 180; frame <= end && frame <= 500; ++frame) {
            print frame, "waypoint-formation-sample Player_1 WaitFormationNear 0"
            print frame, "waypoint-formation-sample Player_1 WaitFormationFar 0"
            print frame, "waypoint-formation-sample Player_1 WaitFormationHero 0"
        }
    }'
}

check() {
    grep -q 'create Player_1 WaitFormationNear' "$out/commands.txt" || return 0
    covers 500 || return 0
    awk -v checkpoint="$save_frame" '
        $1 == "TEST" && $2 == "waypoint-formation" {
            phase[$3, $4] = $5
            route[$3, $4] = $6
            flag[$3, $4] = $7
            speed[$3, $4] = $8
        }
        END {
            near = "WaitFormationNear"
            far = "WaitFormationFar"
            hero = "WaitFormationHero"
            waiter = phase[checkpoint, near] == 1 ? near : far
            peer = waiter == near ? far : near
            if (!route[checkpoint, waiter] || !route[checkpoint, peer] ||
                !flag[checkpoint, waiter] || !flag[checkpoint, peer] || flag[checkpoint, hero] ||
                phase[checkpoint, waiter] != 1 || phase[checkpoint, peer] != 0) {
                print "Expected two opted-in hordes at different waypoint phases at the checkpoint"
                exit 1
            }
            for (frame = checkpoint; frame <= 500; ++frame) {
                if (!stopped && phase[frame, waiter] == 1 && speed[frame, waiter] < 0.0001 &&
                    phase[frame, peer] == 0 && speed[frame, peer] > 0.01 && speed[frame, hero] > 0.01)
                    stopped = frame
                if (stopped && !released && phase[frame, peer] != 0 && speed[frame, waiter] > 0.01)
                    released = frame
            }
            if (!stopped || !released) {
                printf "Expected held horde to stop and then move after its peer reached the waypoint. Stopped %d, released %d\n", stopped, released
                exit 1
            }
            printf "%s stopped at %d and resumed at %d\n", waiter, stopped, released
        }
    ' "$log"
}
