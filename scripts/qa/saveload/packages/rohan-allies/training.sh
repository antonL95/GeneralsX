check() {
    for frame in 236 243 390; do
        if ! covers "$frame"; then
            continue
        fi
        awk -v frame="$frame" '
            function abs(value) { return value < 0 ? -value : value }
            BEGIN {
                x[0] = 60
                x[1] = 18.541
                x[2] = -48.541
                y[0] = 0
                y[1] = 57.063
                y[2] = 35.267
                angle[0] = 0
                angle[1] = 1.256637
                angle[2] = 2.513274
            }
            $1 == "TEST" && $2 == "rohan-allies" && $3 == frame && $6 == "anchor" {
                horde = $5
                seen[horde]++
                if (abs($8 - x[horde]) > 0.1 || abs($9 - y[horde]) > 0.1 || abs($11 - angle[horde]) > 0.001) {
                    print "rohan-allies: unexpected anchor transform", $0 > "/dev/stderr"
                    failed = 1
                }
            }
            END {
                for (horde = 0; horde < 3; ++horde)
                    if (seen[horde] != 1) {
                        print "rohan-allies: missing or duplicate anchor", horde, "at", frame > "/dev/stderr"
                        failed = 1
                    }
                exit failed
            }
        ' "$log" || return 1
    done
}

compare() {
    if [ "$save_frame" -le 390 ] && [ "$end" -ge 390 ]; then
        grep '^TEST rohan-allies 390 ' "$out/continue.log" > "$out/rohan-allies-continue.txt"
        grep '^TEST rohan-allies 390 ' "$out/resume.log" > "$out/rohan-allies-resume.txt"
        diff -u "$out/rohan-allies-continue.txt" "$out/rohan-allies-resume.txt"
    fi
}
