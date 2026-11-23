prepare() {
    awk 'BEGIN {
        for (frame = 221; frame <= 440; ++frame)
            print frame, "wizard-blast-probe Player_1 WizardBlastCaster 0 PlyrCreeps:WizardBlastTarget:0"
        for (frame = 521; frame <= 650; ++frame)
            print frame, "wizard-blast-probe Player_1 WizardBlastCaster 0 PlyrCreeps:WizardBlastTarget:0"
        for (frame = 720; frame <= 850; ++frame)
            print frame, "wizard-blast-probe Player_1 RivendellGandalfTheGrey_Summoned 0 PlyrCreeps:WizardBlastTarget:0"
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
            expected[524] = "unpack=0 prepare=0 weapon2=0 weapon3=0"
            expected[525] = "unpack=1 prepare=0 weapon2=0 weapon3=1"
            expected[584] = "unpack=1 prepare=0 weapon2=0 weapon3=1"
            expected[585] = "unpack=0 prepare=1 weapon2=0 weapon3=1"
            expected[590] = "unpack=0 prepare=1 weapon2=0 weapon3=1 hp=10000.0"
            expected[591] = "unpack=0 prepare=0 weapon2=0 weapon3=1 hp=10000.0"
            expected[608] = "hp=7900.0"
            expected[609] = "unpack=0 prepare=0 weapon2=0 weapon3=0"
            expected[723] = "unpack=0 prepare=0"
            expected[724] = "unpack=1 prepare=0"
            expected[777] = "unpack=1 prepare=0 weapon2=0 weapon3=0 hp=10000.0"
            expected[778] = "unpack=0 prepare=0 weapon2=0 weapon3=0 hp=9940.0"
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
            if ((frame == 361 || frame == 591) && $NF != "packing=1")
                next
            if (frame == 590 || frame == 591) {
                split($18, facing, "=")
                if (facing[2] <= 0)
                    next
            }
            if (frame >= 724 && frame <= 778 && $17 != "weapon1=1")
                next
            if ((frame in expected) && index($0, expected[frame]))
                delete expected[frame]
        }
        END {
            for (frame in expected) {
                if (frame >= first && frame <= end) {
                    print "Missing expected cast state at frame", frame, expected[frame]
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
