prepare() {
    case "${FOLLOWUP_QUEUE_CASE:-}" in
        '') return 0 ;;
        archer-first|heroes-first) last_probe=1630 ;;
        upgrade-first|hero-first) last_probe=750 ;;
        shared-cap|upgrades-completed) last_probe=81 ;;
        *) printf 'Unknown production queue case: %s\n' "$FOLLOWUP_QUEUE_CASE" >&2; return 1 ;;
    esac
    if [ "$save_frame" -ne 80 ] || [ "$end" -lt "$last_probe" ]; then
        printf 'Production queue case %s requires save frame 80 and an end frame of at least %s\n' \
            "$FOLLOWUP_QUEUE_CASE" "$last_probe" >&2
        return 1
    fi
    printf '%s\n' '4 pw-override Player_1 - 0 -' '7 player Player_1 - 0' '8 pw-cp Player_1 - 0'
    cat "$package_dir/queue-$FOLLOWUP_QUEUE_CASE.txt"
}

check_queue() {
    if ! covers "$1"; then
        return 0
    fi
    actual=$(awk -v frame="$1" '
        $1 == "TEST" && $2 == "production" && $3 == frame &&
        ($4 == "MenFortressCitadel" || $4 == "RivendellDunedainGathering") {
            split($7, progress, "=")
            print $6, (progress[2] > 0 ? "running" : "waiting")
        }
    ' "$log")
    if [ "$actual" != "$expected" ]; then
        printf '%s queue at frame %s\nExpected:\n%s\nActual:\n%s\n' "$phase" "$1" "$expected" "$actual" >&2
        return 1
    fi
    printf '%s queue at frame %s\n%s\n' "$phase" "$1" "$actual"
}

check_hero() {
    if covers "$1"; then
        grep -E "^TEST hero $1 Player_1 [0-9]+ $2 state=[a-z]+ alive=$3 queued=$4 " "$log" || {
            printf 'Unexpected %s roster state at frame %s, expected alive=%s queued=%s\n' "$2" "$1" "$3" "$4" >&2
            return 1
        }
    fi
}

check() {
    waiting_porters=0
    case "${FOLLOWUP_QUEUE_CASE:-}" in
        archer-first)
            expected='ArnorRangerHorde running
ElvenElladan running
ElvenElrohir running
ArnorRangerHorde waiting' ;;
        heroes-first)
            expected='ElvenElladan running
ElvenElrohir running
ArnorRangerHorde waiting' ;;
        upgrade-first)
            expected='Upgrade_MenFortressBanners running
GondorPippin running' ;;
        hero-first)
            expected='GondorPippin running
Upgrade_MenFortressBanners waiting' ;;
        shared-cap)
            expected='Upgrade_MenFortressBanners running
Upgrade_MenFortressHouseOfHealing waiting
Upgrade_MenFortressBoilingOil waiting
Upgrade_GoodFortressFlamingMunitionsTrigger waiting
Upgrade_MenFortressNumenorStoneworkTrigger waiting
GondorPippin running
GondorForlongNew running
GondorBeregond running
GondorDenethor running
GondorFaramir running
GondorBoromir running
GondorGandalf running
MenPorter running'
            waiting_porters=7 ;;
        upgrades-completed)
            expected='MenPorter running
GondorPippin running
GondorForlongNew running
GondorBeregond running
GondorDenethor running
GondorFaramir running
GondorBoromir running
GondorGandalf running'
            waiting_porters=12 ;;
    esac
    if [ -n "${FOLLOWUP_QUEUE_CASE:-}" ]; then
        case "$FOLLOWUP_QUEUE_CASE" in
            archer-first|heroes-first) faction=FactionArnor ;;
            *) faction=FactionMen ;;
        esac
        if covers 7; then
            grep -E "^TEST player 7 Player_1 template=$faction " "$log" || return 1
        fi
        if covers 8; then
            grep -E '^PW cp frame=8 used=[0-9]+ total=1234 maximum=2345$' "$log" || return 1
        fi
        while [ "$waiting_porters" -gt 0 ]; do
            expected="$expected
MenPorter waiting"
            waiting_porters=$((waiting_porters - 1))
        done
        check_queue 79 && check_queue 81 || return 1
    fi
    case "${FOLLOWUP_QUEUE_CASE:-}" in
        upgrades-completed)
            if covers 19; then
                awk '/^TEST check-upgrade frame=19/ { count++; if ($4 == "found=1") complete++ }
                    END {
                        if (count == 6 && complete == 6) exit 0
                        printf "Expected six completed upgrades at frame 19, found %d of %d\n", complete, count
                        exit 1
                    }' "$log" || return 1
            fi ;;
        upgrade-first)
            check_queue 25 || return 1
            expected=''
            check_queue 750 && check_hero 750 GondorPippin 1 0 || return 1 ;;
        hero-first)
            check_queue 25 || return 1
            expected='Upgrade_MenFortressBanners running'
            check_queue 750 && check_hero 750 GondorPippin 1 0 || return 1 ;;
        heroes-first)
            expected='ElvenElrohir running
ArnorRangerHorde waiting'
            check_queue 1535 && check_hero 1535 ElvenElladan 1 0 &&
                check_hero 1535 ElvenElrohir 0 1 || return 1 ;;
    esac
    case "${FOLLOWUP_QUEUE_CASE:-}" in
        archer-first|heroes-first)
            if covers 14; then
                expect 'TEST production-queue-upgrade 14 Upgrade_SonsOfElrond complete=0' || return 1
            fi
            if covers 18; then
                expect 'TEST production-queue-upgrade 18 Upgrade_SonsOfElrond complete=1' || return 1
            fi
            expected='ArnorRangerHorde running'
            check_queue 1630 && check_hero 1630 ElvenElladan 1 0 &&
                check_hero 1630 ElvenElrohir 1 0 || return 1 ;;
    esac
}
