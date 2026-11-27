check() {
    if ! grep -q ' create PlyrCreeps SalvageParityBanner ' "$out/continue.txt"; then
        return
    fi
    if covers 330; then
        awk -v phase="$phase" '
        BEGIN {
            expected["SalvageParityBanner"] = "reward=resources resources=117 levels=0 banner=0"
            expected["SalvageParityResources"] = "reward=resources resources=100 levels=0 banner=0"
            expected["SalvageParityExperience"] = "reward=experience resources=0 levels=1 banner=0"
            expected["SalvageParityPlayer"] = "reward=upgrade resources=0 levels=0 banner=0"
            expected["SalvageParityObject"] = "reward=none resources=0 levels=0 banner=0"
        }
        $1 == "TEST" && $2 == "crate" && substr($4, 7) in expected {
            name = substr($4, 7)
            seen[name]++
            outcome[name] = $(NF-3) " " $(NF-2) " " $(NF-1) " " $NF
            print $0
        }
        END {
            for (name in expected) {
                if (seen[name] != 1 || outcome[name] != expected[name]) {
                    print "Unexpected salvage reward", phase, name, seen[name] + 0, outcome[name]
                    failed = 1
                }
            }
            exit failed
        }' "$log"
    fi
}
