check() {
    covers 1300 || return 0
    awk '
        $1 == "TEST" && $2 == "horde-turn" && $3 == 1300 && $4 == "GondorFountainGuardHorde" {
            print
            turned = ($8 > 0.785398 || $8 < -0.785398)
            passed = (turned && $5 == 15 && $6 == 0 && $7 <= 16 && $16 == 0)
        }
        END { exit !passed }
    ' "$log"
}
