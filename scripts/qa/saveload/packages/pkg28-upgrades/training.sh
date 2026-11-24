check() {
    if covers 224; then
        expect 'TEST pkg28 parent 224 Pkg28Tower live=0 dead=0 granted=0 obsolete=1 sold=0'
    fi
    if covers 235; then
        expect 'TEST pkg28 parent 235 Pkg28Control live=1 dead=0 granted=0 obsolete=0 sold=0'
        expect 'TEST pkg28 child 235 Pkg28Control Pkg28Payload dead=0 anim=0 offset=0.0,0.0,0.0 angle=1.571 opacity=1.000 weapon=0 radius=3.0'
    fi
    for probe in 243 245; do
        if covers "$probe"; then
            expect "TEST pkg28 parent $probe Pkg28SavedTower live=1 dead=0 granted=1 obsolete=0 sold=0"
        fi
    done
    if covers 248; then
        expect 'TEST pkg28 parent 248 Pkg28SavedTower live=0 dead=1 granted=1 obsolete=0 sold=0'
        awk '$1 == "TEST" && $2 == "pkg28" && $3 == "child" && $4 == 248 && $5 == "Pkg28SavedTower" &&
            $7 == "dead=1" && $8 == "anim=1" { found = 1 } END { exit !found }' "$log" || return
    fi
    for probe in 243 251 257; do
        if covers "$probe"; then
            expect "TEST pkg28 parent $probe Pkg28Tower live=0 dead=0 granted=0 obsolete=1 sold=0"
        fi
    done
    for probe in 268 272 286 355; do
        if covers "$probe"; then
            expect "TEST pkg28 parent $probe Pkg28Tower live=1 dead=0 granted=1 obsolete=0 sold=0"
        fi
    done
    if covers 270; then
        expect 'TEST pkg28 child 270 Pkg28ShortFade Pkg28Payload dead=0 anim=0 offset=0.0,0.0,0.0 angle=1.571 opacity=1.000 weapon=0 radius=3.0'
        expect 'TEST pkg28 parent 270 Pkg28Control live=1 dead=0 granted=0 obsolete=1 sold=0'
    fi
    if covers 279; then
        expect 'TEST pkg28 parent 279 Pkg28Control live=1 dead=0 granted=0 obsolete=0 sold=0'
    fi
    awk -v first="$first" -v last="$end" '
        $1 == "TEST" && $2 == "pkg28" && $3 == "child" && $5 == "Pkg28Tower" {
            opacity = substr($11, 9) + 0
            if ($4 == 268 || $4 == 272) {
                if ($7 == "dead=0" && $9 == "offset=0.0,20.0,0.0" &&
                    ($10 == "angle=3.142" || $10 == "angle=-3.142") &&
                    opacity > 0 && opacity < 1 && $12 == "weapon=0" && $13 == "radius=3.0")
                    passed[$4]++
            }
            if ($4 == 286 && opacity == 1)
                passed[$4]++
            if (($4 == 292 || $4 == 361) && $7 == "dead=1" && $8 == "anim=1")
                passed[$4]++
            if ($4 == 304 && $7 == "dead=1" && $8 == "anim=0")
                passed[$4]++
        }
        END {
            split("268 272 286 292 304 361", probes, " ")
            for (i = 1; i <= 6; i++) {
                frame = probes[i]
                if (frame >= first && frame <= last && passed[frame] != 1) {
                    print "pkg28: invalid spawn, fade or death state at frame", frame > "/dev/stderr"
                    failed = 1
                }
            }
            exit failed
        }
    ' "$log" || return
    if covers 324; then
        expect 'TEST pkg28 parent 324 Pkg28Tower live=0 dead=0 granted=0 obsolete=0 sold=0'
    fi
    if covers 361; then
        expect 'TEST pkg28 parent 361 Pkg28Tower live=0 dead=1 granted=1 obsolete=0 sold=1'
    fi
    if covers 347; then
        expect 'TEST pkg28 parent 347 Pkg28Control live=1 dead=0 granted=0 obsolete=0 sold=1'
    fi
    if covers 379; then
        expect 'TEST pkg28 parent 379 Pkg28OrcPit live=2 dead=0 granted=0 obsolete=0 sold=0'
        expect 'TEST pkg28 parent 379 Pkg28Wall live=1 dead=0 granted=0 obsolete=0 sold=0'
    fi
    if covers 382; then
        expect 'TEST pkg28 parent 382 Pkg28OrcPit live=2 dead=0 granted=0 obsolete=0 sold=0'
        expect 'TEST pkg28 parent 382 Pkg28Wall live=0 dead=1 granted=0 obsolete=0 sold=0'
        awk '$1 == "TEST" && $2 == "pkg28" && $3 == "child" && $4 == 382 && $5 == "Pkg28Wall" &&
            $6 == "Pkg28Trebuchet" && $7 == "dead=1" && $8 == "anim=1" { found = 1 } END { exit !found }' "$log" || return
    fi
    if covers 750; then
        expect 'TEST pkg28 parent 750 Pkg28Wall live=0 dead=0 granted=0 obsolete=0 sold=0'
    fi
}

compare() {
    for run in continue resume; do
        awk -v start="$save_frame" '$1 == "TEST" && $2 == "pkg28" && $4 >= start' "$out/$run.log" > "$out/pkg28-$run.txt"
    done
    diff -u "$out/pkg28-continue.txt" "$out/pkg28-resume.txt"
}
