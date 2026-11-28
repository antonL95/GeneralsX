check() {
    awk -v first="$first" -v end="$end" '
        function fail(msg) { print msg > "/dev/stderr"; failed = 1 }
        function covered(frame) { return frame >= first && frame <= end }
        function abs(v) { return v < 0 ? -v : v }
        function req(frame, arm, cond, dis, mod, neg, k) {
            if (!covered(frame)) return
            k = frame SUBSEP "RohanTheoden"
            if (!(k in seen)) { fail("missing timed-modifier-duration probe frame=" frame); return }
            if (abs(p_arm[k] - arm) > 0.02) fail("frame=" frame " armor=" p_arm[k] " expected=" arm)
            if (p_cond[k] != cond) fail("frame=" frame " cond=" p_cond[k] " expected=" cond)
            if (p_dis[k] != dis) fail("frame=" frame " disabled=" p_dis[k] " expected=" dis)
            if (mod != "" && index(p_lists[k], mod) == 0) fail("frame=" frame " lists=" p_lists[k] " missing=" mod)
            if (neg != "" && index(p_lists[k], neg) != 0) fail("frame=" frame " lists=" p_lists[k] " still has=" neg)
        }
        $1 == "TIMED" && $2 == "probe" {
            for (i = 3; i <= NF; ++i) {
                split($i, kv, "="); key = kv[1]; val = substr($i, length(key) + 2)
                if (key == "frame") f = val + 0
                else if (key == "template") t = val
                else if (key == "armor") ar = val + 0
                else if (key == "cond") cd = val + 0
                else if (key == "disabled") dl = val + 0
                else if (key == "lists") ls = val
            }
            k = f SUBSEP t; seen[k] = 1; p_arm[k] = ar; p_cond[k] = cd; p_dis[k] = dl; p_lists[k] = ls
        }
        END {
            req(244, 0.50, 1, 0, "AOTRTheodenGloriousCharge(", "")
            req(680, 0.50, 1, 0, "AOTRTheodenGloriousCharge(", "")
            req(750, 0.50, 1, 1, "AOTRTheodenGloriousCharge(", "")
            req(860, 1.00, 0, 1, "[]", "AOTRTheodenGloriousCharge")
            req(920, 1.00, 0, 0, "[]", "AOTRTheodenGloriousCharge")
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        grep -E '^TIMED .*template=RohanTheoden ' "$out/$phase.log" > "$out/timed-mod-$phase.txt" || true
    done
    diff -u "$out/timed-mod-continue.txt" "$out/timed-mod-resume.txt"
}
