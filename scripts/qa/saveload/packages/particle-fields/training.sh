check() {
    for frame in 240 250; do
        if covers "$frame"; then
            awk -v frame="$frame" '
                $1 == "TEST" && $2 == "particle-live" && $3 == "frame=" frame && $5 == "name=TestParticleRadiusGrowth" {
                    for (i = 1; i <= NF; i++) {
                        if ($i ~ /^xyz=/) {
                            sub(/^xyz=/, "", $i)
                            split($i, p, ",")
                            radius = sqrt((p[1] - 2520)^2 + (p[2] - 1010)^2)
                            expected = 10 + frame - 200
                            if (radius < expected - 5 || radius > expected + 5) exit 1
                            found = 1
                        }
                    }
                }
                END { if (!found) exit 1 }
            ' "$log" || { echo "Cylinder radius missing or incorrect at frame $frame"; return 1; }
        fi
    done
}
