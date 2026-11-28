check() {
    awk -v start="${save_frame:-0}" -v end="${end:-999999}" '
        function abs(v) { return v < 0 ? -v : v }
        function covers(f) { return f >= 0 && f <= end }
        $1 == "TEST" && $2 == "aod-horde" && $3 == "probe" {
            for (i = 4; i <= NF; i++) {
                split($i, kv, "=")
                f[kv[1]] = kv[2]
            }
            lbl = f["label"]
            seen[lbl]++
            frm[lbl] = f["frame"] + 0
            cnt[lbl] = f["count"] + 0
            maxOff[lbl] = f["maxOff"] + 0
            maxAlt[lbl] = f["maxAlt"] + 0
            m0y[lbl] = f["m0y"] + 0
            m0z[lbl] = f["m0z"] + 0
            m0ry[lbl] = f["m0ry"] + 0
        }
        END {
            if (!seen["move1"] || !seen["move2"] || !seen["bump2"] || !seen["settle"]) {
                print "aod-horde: missing post-save probes" > "/dev/stderr"
                exit 1
            }
            for (lbl in seen) {
                if (cnt[lbl] < 14) {
                    printf "aod-horde: lost too many members at %s: %d\n", lbl, cnt[lbl] > "/dev/stderr"
                    exit 1
                }
            }
            if (seen["stand1"] && seen["stand3"]) {
                if (abs(m0ry["stand1"]) <= 0.15 || maxAlt["stand1"] <= 0.25) {
                    printf "aod-horde: still wobble missing (m0ry=%.4f maxAlt=%.4f)\n", m0ry["stand1"], maxAlt["stand1"] > "/dev/stderr"
                    exit 1
                }
                if (abs(m0y["stand1"] - m0y["stand3"]) + abs(m0z["stand1"] - m0z["stand3"]) <= 0.01) {
                    printf "aod-horde: still member did not oscillate between stand1 and stand3\n" > "/dev/stderr"
                    exit 1
                }
            }
            if (abs(m0ry["move2"]) <= 1.5 || maxAlt["move2"] <= 2.0) {
                printf "aod-horde: moving wobble too small (m0ry=%.4f maxAlt=%.4f)\n", m0ry["move2"], maxAlt["move2"] > "/dev/stderr"
                exit 1
            }
            peakBump = maxAlt["bump1"]
            if (maxAlt["bump2"] > peakBump) peakBump = maxAlt["bump2"]
            if (maxAlt["bump3"] > peakBump) peakBump = maxAlt["bump3"]
            if (maxAlt["bump4"] > peakBump) peakBump = maxAlt["bump4"]
            if (peakBump <= 15.0) {
                printf "aod-horde: large-unit height bump missing (peakBump=%.4f)\n", peakBump > "/dev/stderr"
                exit 1
            }
            if (!seen["decay"] || maxAlt["decay"] >= 10.0) {
                printf "aod-horde: large-unit height bump did not decay (maxAlt=%.4f)\n", maxAlt["decay"] > "/dev/stderr"
                exit 1
            }
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '
            $1 == "TEST" && $2 == "aod-horde" && $3 == "probe" {
                split($4, kv, "=")
                if ((kv[2] + 0) >= first)
                    print
            }
        ' "$out/$phase.log" > "$out/aod-horde-$phase.txt"
    done
    diff -u "$out/aod-horde-continue.txt" "$out/aod-horde-resume.txt"
}
