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

check() {
    expect_sample 228 'structure-rejected count=0 attached=0 bone=-'
    expect_sample 234 'chariot-created-three count=3'
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
}

compare() {
    grep -F 'TEST container-fields frame=247 release-restored ' "$out/continue.log" > "$out/container-fields-release-continued.txt"
    grep -F 'TEST container-fields frame=247 release-restored ' "$out/resume.log" > "$out/container-fields-release-resumed.txt"
    cmp "$out/container-fields-release-continued.txt" "$out/container-fields-release-resumed.txt"
}
