check() {
    if covers 203; then
        expect "TEST attack-move-hotkey frame=203 key=A attack-move=1 needs-target=1"
    fi
    if ! covers 960; then
        return 0
    fi
    awk '
        $1 == "TEST" && $2 == "attack-move" {
            who = substr($3, 5)
            split($4, f, "=")
            split($5, px, "=")
            split($10, mid, "=")
            split($11, e, "=")
            x[who, f[2]] = px[2]
            midHealth[who, f[2]] = mid[2]
            endHealth[who, f[2]] = e[2]
        }
        END {
            bad = 0
            if (midHealth["AttackMoveUnit", 330] != -1 || endHealth["AttackMoveUnit", 330] != 100) bad = 1
            if (endHealth["AttackMoveUnit", 700] != -1) bad = 1
            if (midHealth["AttackMoveHorde", 330] != -1 || endHealth["AttackMoveHorde", 700] != -1) bad = 1
            if (midHealth["AttackMoveUnit", 800] != 100 || x["AttackMoveUnit", 800] < 3180) bad = 1
            if (midHealth["AttackMoveHorde", 960] != 100) bad = 1
            if (midHealth["AttackMoveUnit", 960] != 100) bad = 1
            exit bad
        }
    ' "$log" || {
        printf 'attack-move: attack move did not engage on the way and at the destination, or a plain move did\n' >&2
        return 1
    }
}
