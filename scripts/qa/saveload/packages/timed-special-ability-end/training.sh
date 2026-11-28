check() {
    awk -v first="$first" -v end="$end" '
        function fail(msg) { print msg > "/dev/stderr"; failed = 1 }
        function covered(frame) { return frame >= first && frame <= end }
        function abs(v) { return v < 0 ? -v : v }
        function req(frame, tpl, dmg, arm, spd, cond, mod, neg, k) {
            if (!covered(frame)) return
            k = frame SUBSEP tpl
            if (!(k in seen)) { fail("missing timed-special-ability-end probe frame=" frame " tpl=" tpl); return }
            if (dmg != "-" && abs(p_dmg[k] - dmg) > 0.02) fail("frame=" frame " " tpl " damage=" p_dmg[k] " expected=" dmg)
            if (arm != "-" && abs(p_arm[k] - arm) > 0.02) fail("frame=" frame " " tpl " armor=" p_arm[k] " expected=" arm)
            if (spd != "-" && abs(p_spd[k] - spd) > 0.02) fail("frame=" frame " " tpl " speedMult=" p_spd[k] " expected=" spd)
            if (cond != "-" && p_cond[k] != cond) fail("frame=" frame " " tpl " cond=" p_cond[k] " expected=" cond)
            if (mod != "" && index(p_lists[k], mod) == 0) fail("frame=" frame " " tpl " lists=" p_lists[k] " missing=" mod)
            if (neg != "" && index(p_lists[k], neg) != 0) fail("frame=" frame " " tpl " lists=" p_lists[k] " still has=" neg)
        }
        $1 == "TIMED" && $2 == "probe" {
            for (i = 3; i <= NF; ++i) {
                split($i, kv, "="); key = kv[1]; val = substr($i, length(key) + 2)
                if (key == "frame") f = val + 0
                else if (key == "template") t = val
                else if (key == "damage") dg = val + 0
                else if (key == "armor") ar = val + 0
                else if (key == "speedMult") sp = val + 0
                else if (key == "cond") cd = val + 0
                else if (key == "lists") ls = val
            }
            k = f SUBSEP t; seen[k] = 1; p_dmg[k] = dg; p_arm[k] = ar; p_spd[k] = sp; p_cond[k] = cd; p_lists[k] = ls
        }
        END {
            req(244, "RohanGimli", 2.00, 1.00, 1.75, -1, "Slayer(", "")
            req(1050, "RohanGimli", 2.00, 1.00, 1.75, -1, "Slayer(", "")
            req(1130, "RohanGimli", 1.00, 1.00, 1.00, -1, "[]", "Slayer")
            req(244, "ElvenGlorfindel", 2.00, 1.00, 1.00, 1, "GlorfindelWrath(", "")
            req(280, "ElvenGlorfindel", 2.00, 1.00, 1.00, 0, "GlorfindelWrath(", "")
            req(1050, "ElvenGlorfindel", 2.00, 1.00, 1.00, 0, "GlorfindelWrath(", "")
            req(1130, "ElvenGlorfindel", 1.00, 1.00, 1.00, 0, "[]", "GlorfindelWrath")
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        grep -E '^TIMED .*template=(RohanGimli|ElvenGlorfindel) ' "$out/$phase.log" > "$out/timed-sa-$phase.txt" || true
    done
    diff -u "$out/timed-sa-continue.txt" "$out/timed-sa-resume.txt"
}
