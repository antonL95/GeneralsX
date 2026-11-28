check() {
    awk -v first="$first" -v end="$end" '
        function fail(msg) { print msg > "/dev/stderr"; failed = 1 }
        function covered(frame) { return frame >= first && frame <= end }
        function abs(v) { return v < 0 ? -v : v }
        function req(frame, arm, upg, dis, mod, neg, k) {
            if (!covered(frame)) return
            k = frame SUBSEP "DolAmrothErchirion"
            if (!(k in seen)) { fail("missing timed-upgrade-removal probe frame=" frame); return }
            if (abs(p_arm[k] - arm) > 0.02) fail("frame=" frame " armor=" p_arm[k] " expected=" arm)
            if (p_upg[k] != upg) fail("frame=" frame " upgrade=" p_upg[k] " expected=" upg)
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
                else if (key == "upgrade") up = val + 0
                else if (key == "disabled") dl = val + 0
                else if (key == "lists") ls = val
            }
            k = f SUBSEP t; seen[k] = 1; p_arm[k] = ar; p_upg[k] = up; p_dis[k] = dl; p_lists[k] = ls
        }
        END {
            req(244, 0.57, 1, 0, "ErchirionStarPupilBuff(", "")
            req(1080, 0.57, 1, 1, "ErchirionStarPupilBuff(", "")
            req(1180, 1.00, 0, 1, "[]", "ErchirionStarPupilBuff")
            req(1220, 1.00, 0, 0, "[]", "ErchirionStarPupilBuff")
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        grep -E '^TIMED .*template=DolAmrothErchirion ' "$out/$phase.log" > "$out/timed-upg-$phase.txt" || true
    done
    diff -u "$out/timed-upg-continue.txt" "$out/timed-upg-resume.txt"
}
