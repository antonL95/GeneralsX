prepare() {
    frame=232
    while [ "$frame" -le 477 ]; do
        printf '%s ocl-fade-trace Player_1 RohanRohirrimHorde_Summoned 0\n' "$frame"
        frame=$((frame + 1))
    done
}

check() {
    awk -v first="$first" -v last="$end" '
        function abs(v) { return v < 0 ? -v : v }
        $1 == "TEST" && $2 == "ocl-fade" {
            frame = $3
            horde = $4
            member = $5
            opacity = $6
            if (member == -1)
                anchors[frame]++
            else
                riders[frame, horde]++
            expected = member == -1 || frame >= 274 ? 1 : (frame - 234) / 40
            if ((frame < 235 || abs(opacity - expected) > 0.000001) && !failed++) {
                print "ocl-fade: unexpected hatch or opacity (expected " expected "):", $0 > "/dev/stderr"
            }
        }
        END {
            for (frame = 235; frame <= 477; ++frame) {
                if (frame < first || frame > last)
                    continue
                if (anchors[frame] != 3 && !failed++)
                    print "ocl-fade: expected three anchors at", frame > "/dev/stderr"
                for (horde = 0; horde < 3; ++horde)
                    if (riders[frame, horde] != 10 && !failed++)
                        print "ocl-fade: expected ten riders per horde at", frame > "/dev/stderr"
            }
            exit (failed != 0)
        }
    ' "$log"
}

compare() {
    awk -v first="$save_frame" '$1 == "TEST" && $2 == "ocl-fade" && $3 >= first' "$out/continue.log" > "$out/ocl-fade-continue.txt"
    awk -v first="$save_frame" '$1 == "TEST" && $2 == "ocl-fade" && $3 >= first' "$out/resume.log" > "$out/ocl-fade-resume.txt"
    diff -u "$out/ocl-fade-continue.txt" "$out/ocl-fade-resume.txt"
}
