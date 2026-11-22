prepare() {
    for frame in 230 231 232 233 234 235 236 253 254 290 291 292 293 294 295 296 320 321 322 323 324 325 326 340 341 350 351 352 353; do
        for victim in Alive Killed Deleted; do
            printf '%s dominate-revert-probe - DominateRevertVictim%s 0\n' "$frame" "$victim"
        done
    done
}

expect_owner() {
    if covers "$1"; then
        expect "TEST dominate-revert frame=$1 object=DominateRevertVictim$2 state=alive owner=$3"
    fi
}

check() {
    for victim in Alive Killed Deleted; do
        expect_owner 229 "$victim" PlyrCreeps
        expect_owner 245 "$victim" Player_1
        first_converted=$(sed -n "s/^TEST dominate-revert frame=\([0-9]*\) object=DominateRevertVictim$victim state=alive owner=Player_1$/\1/p" "$out/continue.log" | head -n 1)
        if [ -z "$first_converted" ] || [ "$first_converted" -le 230 ] || [ "$first_converted" -gt 236 ]; then
            printf 'No cast observed for %s between frames 230 and 235\n' "$victim" >&2
            return 1
        fi
        cast_frame=$((first_converted - 1))
        delay=60
        if [ "$victim" = Alive ]; then
            delay=120
        fi
        check_frame=$((cast_frame + delay))
        expect_owner "$check_frame" "$victim" Player_1
        expect_owner "$((check_frame + 1))" "$victim" PlyrCreeps
        expect_owner 353 "$victim" PlyrCreeps
        printf 'DominateRevertVictim%s %s cast=%s revert=%s\n' "$victim" "$phase" "$cast_frame" "$((check_frame + 1))"
    done

    expect_owner 254 Killed Player_1
    if covers 293; then
        expect_owner 293 Killed PlyrCreeps
    fi

    if covers 241; then
        expect "TEST dominate-revert frame=241 object=DominateRevertCasterDeleted state=absent owner=-"
        grep -Eq '^TEST dominate-revert frame=241 object=DominateRevertCasterKilled state=(dead|absent) owner=' "$log"
    fi
    if covers 327; then
        expect "TEST dominate-revert frame=327 object=DominateRevertCasterAlive state=alive owner=Player_1"
    fi
}
