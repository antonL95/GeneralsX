check() {
    grep -q ' create Player_1 VictoryFixtureShooter ' "$out/continue.txt" || return 0

    if covers 355; then
        expect 'TEST victory frame=355 player=Player_1 unit=BGGondorFighterHorde map=104.500 cell=104.500 shifted=104.500 rank=1 promoted=0 cheer=0'
        expect 'TEST victory frame=355 player=Player_2 unit=GondorSpearmen_BG map=-95.000 cell=-95.000 shifted=-95.000 rank=1 promoted=0 cheer=0'
    fi
    for frame in 364 365; do
        if covers "$frame"; then
            expect "TEST victory frame=$frame player=Player_1 unit=BGGondorFighterHorde map=82.500 cell=82.500 shifted=82.500 rank=1 promoted=0 cheer=0"
            expect "TEST victory frame=$frame player=Player_2 unit=GondorSpearmen_BG map=-75.000 cell=-75.000 shifted=-75.000 rank=1 promoted=0 cheer=0"
        fi
    done
    if covers 373; then
        expect 'TEST victory frame=373 player=Player_1 unit=BGGondorFighterHorde map=165.220 cell=0.000 shifted=0.000 rank=2 promoted=1 cheer=0'
        expect 'TEST victory frame=373 player=Player_2 unit=GondorSpearmen_BG map=-150.200 cell=0.000 shifted=0.000 rank=1 promoted=0 cheer=0'
    fi
    if covers 378; then
        expect 'TEST victory frame=378 player=Player_1 unit=BGGondorFighterHorde map=165.220 cell=0.000 shifted=0.000 rank=2 promoted=1 cheer=1'
        expect 'TEST victory frame=378 player=Player_2 unit=GondorSpearmen_BG map=-150.200 cell=0.000 shifted=0.000 rank=1 promoted=0 cheer=0'
        expect 'TEST victory frame=378 player=Player_1 unit=GondorArcher_BG map=165.220 cell=0.000 shifted=0.000 rank=1 promoted=0 cheer=0'
        grep -E '^TEST xp-state frame=378 object=[0-9]+ BGGondorFighterHorde rank=2 ' "$log"
    fi
}
