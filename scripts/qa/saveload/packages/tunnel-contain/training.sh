sample() {
    frame=$1
    shift
    if covers "$frame"; then
        grep -F "TEST tunnel frame=$frame $*" "$log" || {
            printf 'Missing in %s at frame %s: %s\n' "$phase" "$frame" "$*" >&2
            return 1
        }
    fi
}

check() {
    sample 182 'PlyrCivilian Slot8TunnelRider allowed=1'
    sample 182 'PlyrCivilian Slot8TunnelBlocked allowed=0 reachable=1'
    sample 182 'Player_1 Slot8TunnelRider allowed=0'
    sample 182 'stock-geometry entry=3450.0,3400.0 approach=3420.0,3400.0 exit=3350.0,3400.0'
    sample 182 'timed-geometry entry=3450.0,3260.0 approach=3350.0,3240.0 exit=3350.0,3260.0'
    sample 239 'first-inside alive=1 contained=1 enclosed=1 unselectable=1'
    sample 239 'second-inside alive=1 contained=1 enclosed=1 unselectable=1'
    sample 239 'blocked-outside alive=1 contained=0 enclosed=0 unselectable=0'
    sample 239 'stock-pips count=2 pips=0'
    sample 239 'timed-pips count=2 pips=1 total=5 full=2'
    sample 244 'pending-at-save count=1'
    sample 244 'passenger-bone assigned=1 positioned=1'
    sample 246 'pending-after-save count=1'
    sample 246 'first-exiting alive=1 contained=0 enclosed=0 unselectable=0'
    sample 246 'second-waiting alive=1 contained=1 enclosed=0 unselectable=1'
    sample 246 'passenger-bone assigned=1 positioned=1'
    sample 259 'between-exits count=1'
    sample 277 'delay-elapsed count=0'
    sample 352 'first-exited alive=1 contained=0 enclosed=0 unselectable=0'
    sample 352 'second-exited alive=1 contained=0 enclosed=0 unselectable=0'
    if covers 352; then
        awk '
            /TEST tunnel frame=352 (first|second)-exited / {
                split($9, x, "=")
                split($10, y, "=")
                if (x[2] >= 3320 && x[2] <= 3380 && y[2] >= 3230 && y[2] <= 3290)
                    arrived++
            }
            END {
                if (arrived != 2)
                    print "Expected both riders to reach the rotated ExitOffset at frame 352" > "/dev/stderr"
                exit arrived != 2
            }
        ' "$log"
    fi
    sample 395 'before-death alive=1 contained=1 enclosed=0 unselectable=1'
    sample 395 'passenger-bone assigned=1 positioned=1'
    sample 410 'after-death alive=0 contained=0'
    sample 460 'before-stock-death alive=1 contained=1 enclosed=1 unselectable=1'
    sample 474 'after-stock-death alive=1 contained=0 enclosed=0 unselectable=0'
}

compare() {
    grep -F 'TEST tunnel frame=246 ' "$out/continue.log" > "$out/tunnel-continued.txt"
    grep -F 'TEST tunnel frame=246 ' "$out/resume.log" > "$out/tunnel-resumed.txt"
    cmp "$out/tunnel-continued.txt" "$out/tunnel-resumed.txt"
}
