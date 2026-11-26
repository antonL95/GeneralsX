prepare() {
    probe=240
    while [ "$probe" -le 580 ] && [ "$probe" -le "$end" ]; do
        printf '%s horde-choke Player_1 HordeChokeGuardHorde 0\n' "$probe"
        probe=$((probe + 1))
    done
}

check() {
    [ "$phase" = continue ] || return 0
    covers 580 || return 0
    awk '
        $1 == "TEST" && $2 == "horde-choke" {
            samples++
            if ($4 != 15) lost = 1
            if ($5 && $11) steering = 1
            if ($5 && $6) rerouting = 1
            if ($7 > throat) throat = $7
            if ($9 > penetration) { penetration = $9; overlapFrame = $3 }
            if ($3 == 580) { arrived = $8; error = $10; stopped = !$5; x = $12; y = $13 }
        }
        END {
            printf "horde-choke samples=%d steering=%d rerouting=%d throat=%d penetration=%.4f frame=%d arrived=%d slotError=%.4f anchor=(%.2f,%.2f)\n", samples, steering, rerouting, throat, penetration, overlapFrame, arrived, error, x, y
            exit !(samples == 341 && !lost && steering && rerouting && throat > 1 && penetration <= 0.01 && arrived == 15 && error <= 20 && stopped)
        }
    ' "$log"
}

compare() {
    awk -v save="$save_frame" '
        $1 == "TEST" && $2 == "horde-choke" && $3 >= save {
            if (FILENAME == ARGV[1]) baseline[$3] = $0
            else if ($0 == baseline[$3]) delete baseline[$3]
            else {
                printf "horde-choke observation differs after reload at frame %d\n", $3
                failed = 1
            }
        }
        END { exit (failed || length(baseline)) ? 1 : 0 }
    ' "$out/continue.log" "$out/resume.log"
}
