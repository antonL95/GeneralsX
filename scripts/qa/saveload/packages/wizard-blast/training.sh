prepare() {
    awk 'BEGIN {
        for (frame = 221; frame <= 440; ++frame)
            print frame, "wizard-blast-probe Player_1 WizardBlastCaster 0 PlyrCreeps:WizardBlastTarget:0"
    }'
}

check() {
    awk -v first="$first" -v end="$end" '
        BEGIN {
            expected[244] = "moving=1 drawmove=1 unpack=0 prepare=0 weapon2=0"
            expected[324] = "unpack=0 prepare=0 weapon2=0"
            expected[325] = "moving=0 drawmove=0 unpack=1 prepare=0 weapon2=1"
            expected[340] = "moving=0 drawmove=0 unpack=1 prepare=0 weapon2=1"
            expected[354] = "unpack=1 prepare=0 weapon2=1"
            expected[355] = "unpack=0 prepare=1 weapon2=1"
            expected[360] = "unpack=0 prepare=1 weapon2=1 weapon3=0 hp=10000.0"
            expected[361] = "unpack=0 prepare=0 weapon2=1 weapon3=0 hp=10000.0"
            expected[367] = "hp=10000.0"
            expected[368] = "hp=9500.0"
            expected[397] = "unpack=0 prepare=0 weapon2=0"
        }
        /^TEST wizard-blast / {
            split($3, f, "=")
            split($5, d, "=")
            frame = f[2] + 0
            if (frame <= 324 && d[2] > 80 && ($9 != "unpack=0" || $10 != "prepare=0" || $11 != "weapon2=0")) {
                print "Cast animation condition before entering range:", $0
                exit 1
            }
            if (frame == 325 && d[2] > 80)
                next
            if (frame == 361 && $NF != "packing=1")
                next
            if ((frame in expected) && index($0, expected[frame]))
                delete expected[frame]
        }
        END {
            for (frame in expected) {
                if (frame >= first && frame <= end) {
                    print "Missing expected Wizard Blast state at", frame, expected[frame]
                    failed = 1
                }
            }
            if (failed)
                exit 1
        }
    ' "$log"
}

compare() {
    for run in continue resume; do
        awk -v checkpoint="$save_frame" '
            /^TEST wizard-blast / {
                split($3, f, "=")
                if (f[2] > checkpoint)
                    print
            }
        ' "$out/$run.log" > "$out/wizard-blast-$run.log"
    done
    diff -u "$out/wizard-blast-continue.log" "$out/wizard-blast-resume.log"
}
