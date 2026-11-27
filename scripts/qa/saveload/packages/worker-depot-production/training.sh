prepare() {
    printf '%s worker-depot-load-levels - - 0 %s/levels.ini\n' 100 "$package_dir" "$save_frame" "$package_dir"
}

probe_depot() {
    grep -E "^TEST worker-depot frame=$1 rank=$2 xp=$3 earned=[0-9]+$" "$log" || {
        printf 'Expected depot rank %s and XP %s at frame %s\n' "$2" "$3" "$1" >&2
        return 1
    }
}

check() {
    if covers 526; then
        probe_depot 526 1 "[01][.]0" || return 1
    fi
    if covers 850; then
        probe_depot 850 2 "9[.]0" || return 1
    fi
    if covers 1250; then
        probe_depot 1250 2 "19[.]0" || return 1
    fi
    return 0
}

compare() {
    [ "$end" -ge 1250 ] || return 0
    awk '
        /^TEST pkg23 cash frame=[0-9]+ label=worker-depot-/ { split($5, label, "="); split($6, amount, "="); cash[label[2]] = amount[2]; cashCount++ }
        /^TEST worker-depot frame=/ { split($6, amount, "="); earned[++earnedCount] = amount[2] }
        END {
            if (cashCount != 3 || earnedCount != 3 ||
                cash["worker-depot-first"] - cash["worker-depot-before"] != 8 ||
                cash["worker-depot-second"] - cash["worker-depot-first"] != 10 ||
                earned[2] - earned[1] != 8 || earned[3] - earned[2] != 10) {
                print "Expected worker deposit and credited earnings of 8 then 10"
                exit 1
            }
            print "Worker deposits and credited earnings: 8 then 10"
        }
    ' "$out/continue.log" || return 1
    if [ "$save_frame" -le 1250 ]; then
        for sample in 'TEST worker-depot frame=1250 ' 'TEST pkg23 cash frame=1250 label=worker-depot-second '; do
            expected=$(grep -F "$sample" "$out/continue.log") || return 1
            grep -Fx "$expected" "$out/resume.log" || {
                printf 'Resumed delivery differs from: %s\n' "$expected" >&2
                return 1
            }
        done
    fi
    return 0
}
