check() {
    if covers 199; then
        expect "WEAPON-UPGRADE button frame=199 actor=ElvenRivendellInfHorde slot=6 name=Command_PurchaseUpgradeArnorBasicTrainingForElves upgrade=Upgrade_ArnorBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
        expect "WEAPON-UPGRADE button frame=199 actor=GondorFighterHorde slot=6 name=Command_PurchaseUpgradeGondorBasicTraining upgrade=Upgrade_GondorBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
    fi
    if covers 201; then
        expect "WEAPON-UPGRADE button frame=201 actor=ElvenRivendellInfHorde slot=6 name=Command_PurchaseUpgradeArnorBasicTrainingForElves upgrade=Upgrade_ArnorBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE button frame=201 actor=GondorFighterHorde slot=6 name=Command_PurchaseUpgradeGondorBasicTraining upgrade=Upgrade_GondorBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
    fi
    if covers 208; then
        expect "WEAPON-UPGRADE result frame=208 actor=ElvenRivendellInfHorde set=ElvenRivSwordsCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
        expect "WEAPON-UPGRADE result frame=208 actor=GondorFighterHorde set=GondorFighterHordeCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
    fi
    if covers 220; then
        expect "WEAPON-UPGRADE button frame=220 actor=HaradBlackNumenoranWarriorsHorde slot=6 name=Command_PurchaseUpgradeAngmarBasicTrainingFake upgrade=Upgrade_AngmarBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
    fi
    if covers 550; then
        expect "WEAPON-UPGRADE result frame=550 actor=ElvenRivendellInfHorde set=ElvenRivSwordsCommandSet_Lvl2 rank=2 banner=1 button=0 queued=0 complete=1" || return
        expect "WEAPON-UPGRADE result frame=550 actor=GondorFighterHorde set=GondorFighterHordeCommandSet rank=2 banner=1 button=1 queued=0 complete=1" || return
    fi
    if covers 320; then
        expect "WEAPON-UPGRADE button frame=320 actor=HaradBlackNumenoranWarriorsHorde slot=6 name=Command_PurchaseUpgradeAngmarBasicTraining upgrade=Upgrade_AngmarBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
    fi
    if covers 325; then
        expect "WEAPON-UPGRADE result frame=325 actor=HaradBlackNumenoranWarriorsHorde set=HaradDarkNumenoreanWarriorsHordeCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
    fi
    if covers 650; then
        grep -E '^WEAPON-UPGRADE result frame=650 actor=HaradBlackNumenoranWarriorsHorde set=HaradDarkNumenoreanWarriorsHordeCommandSet rank=2 banner=[01] button=1 queued=0 complete=1$' "$log" || return
    fi
}
