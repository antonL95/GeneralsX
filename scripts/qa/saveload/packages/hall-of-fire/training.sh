check() {
    while read -r frame button accepted; do
        if covers "$frame"; then
            expect "TEST hall-cast $frame Command_Spawn${button}Dummy $accepted ready=$accepted valid=$accepted water=0 local=1" || return 1
        fi
    done <<'CASTS'
902 Turin 1
904 GilGalad 0
915 GilGalad 1
925 Fingolfin 1
1036 Celebrimbor 1
1052 Elendil 1
1172 Amroth 1
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
            hero[1008] = "turin"
            hero[1021] = "gil-galad"
            hero[1038] = "fingolfin"
            hero[1142] = "celebrimbor"
            hero[1158] = "elendil"
            hero[1278] = "amroth"
        }
        $1 == "TEST" && $2 == "hall-residents" && $4 > 0 && $5 == 0 {
            grounded[$3] = 1
        }
        $1 == "TEST" && $2 == "hall-object" && $5 != "absent" {
            if ($3 == 818 && $4 == "enemy-before" && $7 == 0 && $8 == 0) before = 1
            if ($3 == 839 && $4 == "enemy-launch" && $8 > 0) launch = 1
            if ($5 == 0 && $6 == 0 && $9 == 0) {
                if ($4 == "gil-galad-arrival" && $10 > 0 && $10 < 1) fade = 1
                if (($3 in hero) && $4 == hero[$3] && $10 > 0) placed[$3] = 1
            }
        }
        END {
            require(818, grounded[818], "residents were missing or knocked back before the impact")
            require(818, before, "enemy was missing or already airborne before the impact")
            require(839, grounded[839], "residents were missing or knocked back by the impact")
            require(839, launch, "enemy was not knocked back")
            if (covered(967) && covered(1019))
                require(1019, fade, "Gil-galad never appeared at the marker during his spawn fade")
            for (frame in hero)
                if (hero[frame] != "")
                    require(frame, placed[frame], hero[frame] " did not appear at the marker in the default state")
            exit failed
        }
    ' "$log"
}
