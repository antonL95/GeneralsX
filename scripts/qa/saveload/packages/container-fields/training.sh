expect_sample() {
    sample_frame=$1
    shift
    if covers "$sample_frame"; then
        grep -F "TEST container-fields frame=$sample_frame $*" "$log" || {
            printf 'Missing in %s at frame %s: %s\n' "$phase" "$sample_frame" "$*" >&2
            return 1
        }
    fi
}

expect_displacement() {
    sample_frame=$1
    if covers "$sample_frame"; then
        awk -v sample="frame=$sample_frame $2 busy=$3" '
            /^TEST container-fields busy / && index($0, sample) {
                split($NF, position, "[=,]");
                dx = position[2] - 430; dy = position[3] - 3200;
                displaced = dx * dx + dy * dy > 25;
                print;
            }
            END { if (!displaced) print "Missing displacement at " sample > "/dev/stderr"; exit !displaced }
        ' "$log"
    fi
}

check() {
    expect_sample 228 'structure-rejected count=0 attached=0 bone=-'
    expect_sample 234 'chariot-created-three count=3'
    expect_sample 234 'zero-payload count=1'
    expect_sample 234 'structure-payload count=1'
    expect_sample 234 'seat-first count=0 attached=1 bone=PASSENGER01'
    expect_sample 234 'seat-second count=0 attached=1 bone=-'
    expect_sample 234 'toss-loaded count=0 attached=1'
    expect_sample 242 'foreign-rejected count=0 attached=0 bone=-'
    expect_sample 242 'own-fixed-burned count=0 attached=1 bone=PASSENGER01 enclosed=0 follows-bone=1 health=90.0'
    expect_sample 242 'free count=0 attached=1 bone=PASSENGER01 enclosed=0 follows-bone=0'
    expect_sample 242 'structure-caught count=0 attached=1 bone=- enclosed=1'
    expect_sample 242 'throw-pending count=0 attached=1'
    expect_sample 242 'beast-loaded count=6'
    expect_sample 242 'beast-seat count=0 attached=1 bone=B_GBPASS01'
    expect_sample 242 'chariot-loaded count=2'
    expect_sample 242 'mine-ammo-loaded count=1'
    expect_sample 247 'release-restored count=0 attached=0 bone=-'
    if covers 247; then
        awk '/TEST container-fields frame=247 release-restored / { split($NF, value, "="); found = value[2] > 0 } END { exit !found }' "$log"
    fi
    expect_sample 248 'fixed-restored count=0 attached=1 bone=PASSENGER01 enclosed=0 follows-bone=1 health=90.0'
    expect_sample 248 'throw-restored count=0 attached=1'
    expect_sample 248 'beast-restored count=6'
    expect_sample 248 'beast-seat-restored count=0 attached=1 bone=B_GBPASS01'
    expect_sample 248 'chariot-restored count=2'
    expect_sample 243 'unseated-before-save count=0 attached=1 bone=-'
    expect_sample 248 'unseated-restored count=0 attached=1 bone=-'
    if covers 259; then
        grep -E '^TEST container-fields toss frame=259 physics=1 velocity=[^ ]+ dead=1$' "$log"
        awk '/^TEST container-fields toss frame=259 / { split($6, v, "[=,]"); found = (v[2] != 0 || v[3] != 0 || v[4] != 0) } END { exit !found }' "$log"
    fi
    expect_sample 258 'thrown count=0 attached=0'
    expect_sample 260 'grab-death alive=0'
    expect_sample 260 'mine-ammo-death alive=0'
    expect_sample 260 'chariot-death alive=0'
    expect_sample 265 'released count=0 attached=0 bone=-'
    if covers 265; then
        awk '/TEST container-fields frame=265 released / { split($NF, value, "="); found = value[2] > 0 } END { exit !found }' "$log"
    fi
    if covers 294; then
        grep -E 'TEST container-fields frame=294 release-complete .* render-offset=0.00$' "$log"
    fi
    if covers 243; then
        expect 'TEST container-fields busy frame=243 before-save busy=1 position=430.00,3200.00'
    fi
    if covers 248; then
        expect 'TEST container-fields busy frame=248 after-save busy=1 position=430.00,3200.00'
    fi
    if covers 270; then
        expect 'TEST container-fields busy frame=270 before-clearance busy=1 position=430.00,3200.00'
    fi
    expect_displacement 292 displaced-before-deadline 0
    expect_displacement 320 after-clearance 0
}

compare() {
    grep -F 'TEST container-fields frame=247 release-restored ' "$out/continue.log" > "$out/container-fields-release-continued.txt"
    grep -F 'TEST container-fields frame=247 release-restored ' "$out/resume.log" > "$out/container-fields-release-resumed.txt"
    cmp "$out/container-fields-release-continued.txt" "$out/container-fields-release-resumed.txt"
    for sample_frame in 292 320; do
        if covers "$sample_frame"; then
            grep -F "TEST container-fields busy frame=$sample_frame " "$out/continue.log" > "$out/container-fields-busy-continued.txt"
            grep -F "TEST container-fields busy frame=$sample_frame " "$out/resume.log" > "$out/container-fields-busy-resumed.txt"
            cmp "$out/container-fields-busy-continued.txt" "$out/container-fields-busy-resumed.txt"
        fi
    done
}
