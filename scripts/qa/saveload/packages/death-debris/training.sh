check() {
    if covers 255; then
        expect 'TEST death-debris frame=255 template=DeathDebrisConverted count=1' || return 1
    fi
    if covers 280; then
        expect 'TEST death-debris frame=280 template=DeathDebrisSpawn count=1' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisPiece count=0' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisConverted count=1' || return 1
        expect 'TEST death-debris frame=280 template=DeathDebrisConversionVictim count=0' || return 1
    fi
    if covers 300; then
        awk '
            /^TEST damage frame=300 weapon=DeathDebrisSplashWeapon / && /\(DeathDebrisSplashTarget\)/ {
                for (i = 1; i <= NF; ++i) {
                    if ($i ~ /^victim=/) {
                        split($i, victim, /[=(]/)
                        id = victim[2] + 0
                        if (count && id <= previous)
                            failed = 1
                        previous = id
                        ++count
                    }
                }
            }
            END {
                if (failed || count != 3) {
                    print "Splash damage did not visit all targets in object ID order" > "/dev/stderr"
                    exit 1
                }
            }
        ' "$log" || return 1
    fi
}

compare() {
    for phase in continue resume; do
        grep '^TEST death-debris ' "$out/$phase.log" > "$out/death-debris-$phase.txt"
    done
    diff -u "$out/death-debris-continue.txt" "$out/death-debris-resume.txt"
}
