census() {
    if covers "$1"; then
        expect "TEST summon-rules census frame=$1 object=$2 $3" || return 1
    fi
}

none='count=0 contained=0 timed=0 rank=0 health=0'

check() {
    census 400 HaradTribalAxemenHorde 'count=1 contained=0 timed=0 rank=1 health=1' || return 1
    census 700 HaradWarlordBanner 'count=1 contained=1 timed=0 rank=1 health=500' || return 1
    census 700 HaradTribalAxemen 'count=20 contained=20 timed=0 rank=1 health=2000' || return 1
    census 900 HaradTribalAxemenHorde "$none" || return 1
    census 900 HaradTribalAxemen "$none" || return 1
    census 262 GuldurOrcPikeman "$none" || return 1
    census 400 GuldurRisenDeadHorde 'count=1 contained=0 timed=0 rank=1 health=1' || return 1
    census 400 RohanPeasant "$none" || return 1
    census 400 GuldurWraith 'count=3 contained=0 timed=3 rank=1 health=3000' || return 1
    census 400 MordorBalrog 'count=1 contained=0 timed=1 rank=10 health=5800' || return 1
    census 400 WildGiantBatInterface 'count=1 contained=0 timed=0 rank=1 health=1500' || return 1
    census 400 SpellBookDragonStrikeDragon 'count=1 contained=0 timed=0 rank=1 health=30000' || return 1
    census 1200 SpellBookDragonStrikeDragon "$none" || return 1
    if covers 1200; then
        expect 'TEST health 1200 Player_1 GuldurOrcArcher 0 id=1058 health=95.00 max=125.00 dead=0 pos=800.0,1800.0' || return 1
        expect 'TEST health 1200 Player_1 GuldurOrcArcher 1 id=1059 health=149.88 max=149.88 dead=0 pos=800.0,2200.0' || return 1
    fi
}
