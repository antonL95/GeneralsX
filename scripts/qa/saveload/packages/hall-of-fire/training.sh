check() {
    while read -r frame button accepted; do
        if covers "$frame"; then
            expect "TEST hall-cast $frame Command_Spawn${button}Dummy $accepted ready=$accepted valid=$accepted water=0 local=1" || return 1
        fi
    done <<'CASTS'
330 Turin 1
332 GilGalad 0
350 GilGalad 1
464 Fingolfin 1
575 Celebrimbor 1
685 Elendil 1
794 Amroth 1
CASTS

    awk -v first="$first" -v end="$end" '
        function covered(frame) { return first <= frame && frame <= end }
        function require(frame, found, detail) {
            if (covered(frame) && !found) {
                print "Hall of Fire: " detail " at frame " frame > "/dev/stderr"
                failed = 1
            }
        }
        BEGIN {
            hero[425] = "turin"
            hero[446] = "gil-galad"
            hero[561] = "fingolfin"
            hero[672] = "celebrimbor"
            hero[782] = "elendil"
            hero[891] = "amroth"
        }
        $1 == "TEST" && $2 == "hall-residents" && $4 > 0 && $5 == 0 {
            grounded[$3] = 1
        }
        $1 == "TEST" && $2 == "hall-object" && $5 != "absent" {
            if ($3 == 243 && $4 == "enemy-before" && $7 == 0 && $8 == 0) before = 1
            if ($3 == 264 && $4 == "enemy-launch" && $8 > 0) launch = 1
            if ($5 == 0 && $6 == 0 && $9 == 0) {
                if ($3 == 416 && $4 == "gil-galad-arrival" && $10 > 0 && $10 < 1) fade = 1
                if (($3 in hero) && $4 == hero[$3] && $10 > 0) placed[$3] = 1
            }
        }
        END {
            require(243, grounded[243], "residents were missing or knocked back before the impact")
            require(243, before, "enemy was missing or already airborne before the impact")
            require(264, grounded[264], "residents were missing or knocked back by the impact")
            require(264, launch, "enemy was not knocked back")
            require(416, fade, "Gil-galad was missing, misplaced, upgraded, or outside his spawn fade")
            for (frame in hero)
                if (hero[frame] != "")
                    require(frame, placed[frame], hero[frame] " did not appear at the marker in the default state")
            exit failed
        }
    ' "$log"
}
