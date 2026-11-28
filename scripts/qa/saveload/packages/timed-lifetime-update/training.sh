check() {
    awk -v first="$first" -v end="$end" '
        function fail(msg) { print msg > "/dev/stderr"; failed = 1 }
        function covered(frame) { return frame >= first && frame <= end }
        function req(frame, tpl, alive, min_total, max_total, k) {
            if (!covered(frame)) return
            k = frame SUBSEP tpl
            if (!(k in seen)) { fail("missing timed-lifetime-update count frame=" frame " tpl=" tpl); return }
            if (alive >= 0 && c_alive[k] != alive) fail("frame=" frame " " tpl " alive=" c_alive[k] " expected=" alive)
            if (min_total >= 0 && c_total[k] < min_total) fail("frame=" frame " " tpl " total=" c_total[k] " <" min_total)
            if (max_total >= 0 && c_total[k] > max_total) fail("frame=" frame " " tpl " total=" c_total[k] " >" max_total)
        }
        $1 == "TIMED" && $2 == "count" {
            for (i = 3; i <= NF; ++i) {
                split($i, kv, "="); key = kv[1]; val = substr($i, length(key) + 2)
                if (key == "frame") f = val + 0
                else if (key == "template") t = val
                else if (key == "alive") al = val + 0
                else if (key == "total") tot = val + 0
            }
            k = f SUBSEP t; seen[k] = 1; c_alive[k] = al; c_total[k] = tot
        }
        END {
            req(244, "DunharrowFaithlessDeadHorde_Skeleton", 1, 1, 1)
            req(750, "DunharrowFaithlessDeadHorde_Skeleton", 1, 1, 1)
            req(820, "DunharrowFaithlessDeadHorde_Skeleton", 0, 0, 1)
            req(1100, "DunharrowFaithlessDeadHorde_Skeleton", 0, 0, 0)
            req(1100, "DunharrowFaithlessDead_Skeleton_Summoned", 0, 0, 0)
            exit failed
        }
    ' "$log"
}

compare() {
    for phase in continue resume; do
        grep -E '^TIMED .*template=(DunharrowFaithlessDeadHorde_Skeleton|DunharrowFaithlessDead_Skeleton_Summoned) ' "$out/$phase.log" > "$out/timed-lt-$phase.txt" || true
    done
    diff -u "$out/timed-lt-continue.txt" "$out/timed-lt-resume.txt"
}
