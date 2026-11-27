fear_state() {
    if covers "$1"; then
        grep -E "^TEST lua fear state frame=$1 unit=$2 $3" "$log" || return 1
    fi
}

fear_moved_away() {
    if covers "$1"; then
        awk -v frame="$1" -v unit="$2" -v origin_y="$3" '
            $0 ~ "^TEST lua fear state frame=" frame " unit=" unit " " {
                split($10, x, "=")
                split($11, y, "=")
                dx = x[2] - 2990
                dy = y[2] - 2860
                start_dy = origin_y - 2860
                moved = x[1] == "x" && y[1] == "y" && dx * dx + dy * dy > 50 * 50 + start_dy * start_dy
                print
            }
            END { exit !moved }
        ' "$log" || return 1
    fi
}

check() {
    for dispatch in '232 BeAfraidOfGateDamaged' '233 BeTerrified' '234 BeAfraidOfPhial' '235 BeAfraidOfBalrog' '236 BeUncontrollablyAfraid' '238 BeAfraidOfGateDamaged' '300 BeAfraidOfBalrog'; do
        set -- $dispatch
        if covers "$1"; then
            expect "TEST lua fear dispatch frame=$1 event=$2 ran=1" || return 1
        fi
    done
    fear_state 232 LuaFearCower 'afraid=0 terror=0 uncontrolled=0 ' || return 1
    fear_state 242 LuaFearCower 'afraid=1 terror=0 uncontrolled=0 x=2940\.000 y=2800\.000 angle=0\.87[0-9] ' || return 1
    fear_state 243 LuaFearCower 'afraid=0 terror=0 uncontrolled=0 x=2940\.000 y=2760\.000 ' || return 1
    fear_state 246 LuaFearCower 'afraid=1 terror=0 uncontrolled=0 ' || return 1
    fear_state 246 LuaFearPanic 'afraid=1 terror=1 uncontrolled=0 ' || return 1
    fear_state 246 LuaFearUncontrolled 'afraid=1 terror=0 uncontrolled=1 x=2940\.000 y=2920\.000 ' || return 1
    fear_state 246 LuaFearUncontrolledBacks 'afraid=1 terror=0 uncontrolled=1 x=2940\.000 y=2710\.000 angle=1\.24[0-9] ' || return 1
    for frame in 243 246; do
        if covers "$frame"; then
            awk -v frame="$frame" '
                $1 == "TEST" && $2 == "locomotor-fields" && $3 == frame && $4 == "LuaFearHorde" {
                    print
                    backing = $6 == 1 && $7 < 0
                }
                END { exit !backing }
            ' "$log" || return 1
        fi
    done
    if covers 274; then
        grep -E '^TEST locomotor-fields 274 LuaFearHorde 0 0 ' "$log" || return 1
    fi
    fear_state 258 LuaFearCower 'afraid=0 terror=0 uncontrolled=0 .* goalx=2960\.000 goaly=2800\.000$' || return 1
    fear_state 258 LuaFearPanic 'afraid=1 terror=1 uncontrolled=0 ' || return 1
    fear_moved_away 258 LuaFearPanic 2860 || return 1
    fear_state 258 LuaFearUncontrolled 'afraid=1 terror=0 uncontrolled=1 x=2940\.000 y=2920\.000 ' || return 1
    fear_state 259 LuaFearCower 'afraid=0 terror=0 uncontrolled=0 .* goalx=2960\.000 goaly=2760\.000$' || return 1
    fear_state 306 LuaFearCower 'afraid=1 terror=0 uncontrolled=0 ' || return 1
    fear_state 400 LuaFearPanic 'afraid=1 terror=1 uncontrolled=0 ' || return 1
    fear_state 420 LuaFearPanic 'afraid=0 terror=0 uncontrolled=0 ' || return 1
    fear_state 460 LuaFearPanic 'afraid=0 terror=0 uncontrolled=0 .* goalx=2960\.000 goaly=2860\.000$' || return 1
    fear_state 675 LuaFearUncontrolled 'afraid=1 terror=0 uncontrolled=1 x=2940\.000 y=2920\.000 ' || return 1
    fear_state 675 LuaFearUncontrolledBacks 'afraid=1 terror=0 uncontrolled=1 ' || return 1
    fear_state 715 LuaFearUncontrolled 'afraid=0 terror=0 uncontrolled=0 ' || return 1
    fear_state 715 LuaFearUncontrolledBacks 'afraid=0 terror=0 uncontrolled=0 ' || return 1
    fear_state 726 LuaFearUncontrolled 'afraid=0 terror=0 uncontrolled=0 .* goalx=2960\.000 goaly=2920\.000$' || return 1
    if covers 232 && covers 726; then
        printf 'LUA_BULK\t%s\t%s\t%s\tscripted\t%s\tran\t\teffect verified\tsample\n' \
            GondorFighterFunctions BeAfraidOfGateDamaged GondorFighterBecomeAfraidOfGateDamaged LuaFearCower \
            MordorFighterFunctions BeAfraidOfPhial MordorFighterBecomeAfraidOfPhial LuaFearUncontrolled \
            MordorFighterFunctions BeUncontrollablyAfraid MordorFighterBecomeUncontrollablyAfraid LuaFearUncontrolledBacks \
            GondorFighterFunctions BeAfraidOfBalrog BecomeAfraidOfBalrog LuaFearCower
    fi
}
