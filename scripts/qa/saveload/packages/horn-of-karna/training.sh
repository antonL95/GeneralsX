check() {
    awk -v first="$first" -v end="$end" '
        function fail(msg) {
            print msg > "/dev/stderr"
            failed = 1
        }
        function covered(frame) {
            return frame >= first && frame <= end
        }
        function req_probe(frame, role, tpl, dmg, spd, dis, cond, mod, neg_mod, k) {
            if (!covered(frame))
                return
            k = frame SUBSEP role SUBSEP tpl
            if (!(k in seen_probe)) {
                fail("missing TIMED probe frame=" frame " role=" role " template=" tpl)
                return
            }
            if (dmg != "-" && abs(p_dmg[k] - dmg) > 0.02)
                fail("frame=" frame " " tpl " damage=" p_dmg[k] " expected=" dmg)
            if (spd != "-" && abs(p_spd[k] - spd) > 0.02)
                fail("frame=" frame " " tpl " speedMult=" p_spd[k] " expected=" spd)
            if (dis != "-" && p_dis[k] != dis)
                fail("frame=" frame " " tpl " dissident=" p_dis[k] " expected=" dis)
            if (cond != "-" && p_cond[k] != cond)
                fail("frame=" frame " " tpl " cond=" p_cond[k] " expected=" cond)
            if (mod != "" && index(p_lists[k], mod) == 0)
                fail("frame=" frame " " tpl " lists=" p_lists[k] " missing=" mod)
            if (neg_mod != "" && index(p_lists[k], neg_mod) != 0)
                fail("frame=" frame " " tpl " lists=" p_lists[k] " still has=" neg_mod)
        }
        function req_count(frame, tpl, alive, total, k) {
            if (!covered(frame))
                return
            k = frame SUBSEP tpl
            if (!(k in seen_count)) {
                fail("missing TIMED count frame=" frame " template=" tpl)
                return
            }
            if (alive != "-" && c_alive[k] != alive)
                fail("frame=" frame " " tpl " alive=" c_alive[k] " expected=" alive)
            if (total != "-" && c_total[k] != total)
                fail("frame=" frame " " tpl " total=" c_total[k] " expected=" total)
        }
        function abs(v) { return v < 0 ? -v : v }
        $1 == "TIMED" && $2 == "probe" {
            for (i = 3; i <= NF; ++i) {
                split($i, kv, "=")
                key = kv[1]
                val = substr($i, length(key) + 2)
                if (key == "frame") f = val + 0
                else if (key == "role") r = val
                else if (key == "template") t = val
                else if (key == "damage") dg = val + 0
                else if (key == "speedMult") sp = val + 0
                else if (key == "dissident") ds = val
                else if (key == "cond") cd = val + 0
                else if (key == "lists") ls = val
            }
            if (r == "object" && t == "HaradTribalAxemenHorde") {
                k = f SUBSEP r SUBSEP t SUBSEP (cd >= 0 ? "0" : "1")
            } else {
                k = f SUBSEP r SUBSEP t
            }
            seen_probe[k] = 1
            p_dmg[k] = dg
            p_spd[k] = sp
            p_dis[k] = ds
            p_cond[k] = cd
            p_lists[k] = ls
        }
        $1 == "TIMED" && $2 == "count" {
            for (i = 3; i <= NF; ++i) {
                split($i, kv, "=")
                key = kv[1]
                val = substr($i, length(key) + 2)
                if (key == "frame") f = val + 0
                else if (key == "template") t = val
                else if (key == "alive") al = val + 0
                else if (key == "total") tot = val + 0
            }
            k = f SUBSEP t
            seen_count[k] = 1
            c_alive[k] = al
            c_total[k] = tot
        }
        END {
            req_probe(285, "object", "HaradTribalAxemenHorde\0340", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(400, "object", "HaradTribalAxemenHorde\0340", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(1130, "object", "HaradTribalAxemenHorde\0340", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(285, "object", "GondorTowerShieldGuardHorde", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(400, "object", "GondorTowerShieldGuardHorde", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(1130, "object", "GondorTowerShieldGuardHorde", "-", "-", "1/1", 1, "UrshakIntimidationModifier(", "")
            req_probe(1160, "object", "HaradTribalAxemenHorde\0340", "-", "-", "0/0", 0, "", "UrshakIntimidationModifier")
            req_probe(1160, "object", "GondorTowerShieldGuardHorde", "-", "-", "0/0", 0, "[]", "UrshakIntimidationModifier")
            req_probe(400, "object", "HaradTribalAxemenHorde\0341", 1.75, 1.20, "-", "-", "KarnaBloodMagicBuff(", "")
            req_probe(1130, "object", "HaradTribalAxemenHorde\0341", 1.75, 1.20, "-", "-", "KarnaBloodMagicBuff(", "")
            req_probe(1160, "object", "HaradTribalAxemenHorde\0341", 1.75, 1.20, "-", "-", "KarnaBloodMagicBuff(", "")
            req_probe(1200, "object", "HaradTribalAxemenHorde\0341", 1.75, 1.20, "-", "-", "KarnaBloodMagicBuff(", "")
            req_probe(1250, "object", "HaradTribalAxemenHorde\0340", 1.00, 1.00, "0/0", "-", "[]", "KarnaBloodMagicBuff")
            req_probe(1250, "object", "HaradTribalAxemenHorde\0341", 1.00, 1.00, "0/0", "-", "[]", "KarnaBloodMagicBuff")
            req_count(285, "KarnaMonster", 3, "-")
            req_count(400, "KarnaMonster", 3, "-")
            req_count(1160, "KarnaMonster", 3, "-")
            req_count(1200, "KarnaMonster", 0, "-")
            req_count(1350, "KarnaMonster", 0, 0)
            req_count(1350, "HaradHornofKarna", 1, "-")
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        grep -E '^TIMED .*template=(HaradTribalAxemenHorde|HaradTribalAxemen|GondorTowerShieldGuardHorde|GondorTowerShieldGuard|KarnaEgg|KarnaMonster|HaradHornofKarna) ' "$out/$phase.log" > "$out/timed-karna-$phase.txt" || true
    done
    diff -u "$out/timed-karna-continue.txt" "$out/timed-karna-resume.txt"
}
