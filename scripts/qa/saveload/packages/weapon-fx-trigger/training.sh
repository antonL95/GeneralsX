prepare() {
    for frame in $(seq 244 360); do
        printf '%s weapon-fx-impact PlyrCreeps WeaponFXTriggerBuilding 0 catapult\n' "$frame"
        printf '%s weapon-fx-impact PlyrCreeps WeaponFXTriggerBuilding 1 trebuchet\n' "$frame"
    done
}

check() {
    if covers 244; then
        expect 'TEST impact-drawable frame=244 case=launcher trigger=NONE'
        expect 'TEST impact-drawable frame=244 case=infantry trigger=NONE'
        expect 'TEST weapon-fx-impact frame=244 case=catapult damaged=0 debris=0'
        expect 'TEST weapon-fx-impact frame=244 case=trebuchet damaged=0 debris=0'
    fi
    if covers 281; then
        expect 'TEST impact-drawable frame=281 case=infantry trigger=CATAPULT_ROCK'
    fi
    if covers 299; then
        expect 'TEST impact-drawable frame=299 case=infantry trigger=CATAPULT_ROCK'
    fi
    if covers 301; then
        expect 'TEST impact-drawable frame=301 case=infantry trigger=NONE'
    fi
    covers 360 || return 0
    awk '
        $1 == "TEST" && $2 == "weapon-fx-impact" {
            split($3, frame, "=")
            split($6, debris, "=")
            if ($4 == "case=catapult" && $5 == "damaged=1" && debris[2] >= 7 && debris[2] % 7 == 0)
                catapult = 1
            if ($4 == "case=trebuchet") {
                if ($5 == "damaged=1")
                    trebuchet = 1
                if (debris[2] != 0) {
                    print "Trebuchet spawned debris at frame " frame[2] > "/dev/stderr"
                    failed = 1
                }
            }
        }
        END {
            if (!catapult)
                print "Missing CATAPULT_ROCK debris on the damaged building" > "/dev/stderr"
            if (!trebuchet)
                print "Trebuchet control did not damage its building" > "/dev/stderr"
            exit (failed || !catapult || !trebuchet)
        }
    ' "$log"
}

compare() {
    grep '^TEST weapon-fx-impact ' "$out/continue.log" | awk -v first="$save_frame" 'substr($3, 7) + 0 >= first' > "$out/weapon-fx-trigger-continue.txt"
    grep '^TEST weapon-fx-impact ' "$out/resume.log" > "$out/weapon-fx-trigger-resume.txt"
    diff -u "$out/weapon-fx-trigger-continue.txt" "$out/weapon-fx-trigger-resume.txt"
}
