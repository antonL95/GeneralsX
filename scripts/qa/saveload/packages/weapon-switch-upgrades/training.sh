check() {
    if covers 139; then
        expect "WEAPON-UPGRADE button frame=139 actor=ElvenRivendellInfHorde slot=6 name=Command_PurchaseUpgradeArnorBasicTrainingForElves upgrade=Upgrade_ArnorBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
        expect "WEAPON-UPGRADE button frame=139 actor=GondorFighterHorde slot=6 name=Command_PurchaseUpgradeGondorBasicTraining upgrade=Upgrade_GondorBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
    fi
    if covers 141; then
        expect "WEAPON-UPGRADE button frame=141 actor=ElvenRivendellInfHorde slot=6 name=Command_PurchaseUpgradeArnorBasicTrainingForElves upgrade=Upgrade_ArnorBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE button frame=141 actor=GondorFighterHorde slot=6 name=Command_PurchaseUpgradeGondorBasicTraining upgrade=Upgrade_GondorBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
    fi
    if covers 148; then
        expect "WEAPON-UPGRADE result frame=148 actor=ElvenRivendellInfHorde set=ElvenRivSwordsCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
        expect "WEAPON-UPGRADE result frame=148 actor=GondorFighterHorde set=GondorFighterHordeCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
    fi
    if covers 220; then
        expect "WEAPON-UPGRADE button frame=220 actor=HaradBlackNumenoranWarriorsHorde slot=6 name=Command_PurchaseUpgradeAngmarBasicTrainingFake upgrade=Upgrade_AngmarBasicTraining state=disabled prerequisite=0 affected=1 offered=0 affordable=1" || return
    fi
    if covers 490; then
        expect "WEAPON-UPGRADE result frame=490 actor=ElvenRivendellInfHorde set=ElvenRivSwordsCommandSet_Lvl2 rank=2 banner=1 button=0 queued=0 complete=1" || return
        expect "WEAPON-UPGRADE result frame=490 actor=GondorFighterHorde set=GondorFighterHordeCommandSet rank=2 banner=1 button=1 queued=0 complete=1" || return
    fi
    if covers 320; then
        expect "WEAPON-UPGRADE button frame=320 actor=HaradBlackNumenoranWarriorsHorde slot=6 name=Command_PurchaseUpgradeAngmarBasicTraining upgrade=Upgrade_AngmarBasicTraining state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
    fi
    if covers 325; then
        expect "WEAPON-UPGRADE result frame=325 actor=HaradBlackNumenoranWarriorsHorde set=HaradDarkNumenoreanWarriorsHordeCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
    fi
    if covers 650; then
        harad_set=HaradDarkNumenoreanWarriorsHordeCommandSet
        if grep -Fxq 'TEST frame 403: grant Player_1 Upgrade_ObjectUnderAIControl' "$log"; then
            harad_set=${harad_set}_ForAI
        fi
        grep -E "^WEAPON-UPGRADE result frame=650 actor=HaradBlackNumenoranWarriorsHorde set=$harad_set rank=2 banner=[01] button=1 queued=0 complete=1$" "$log" || return
    fi
    if covers 233; then
        expect "WEAPON-UPGRADE button frame=233 actor=ElvenRivendellKnightHorde slot=5 name=Command_PurchaseUpgradeArnorForgedBlades upgrade=Upgrade_ArnorForgedBlades state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE result frame=233 actor=ElvenRivendellKnightHorde set=RivendellLancerHordeCommandSet rank=1 banner=0 button=0 queued=0 complete=0" || return
    fi
    if covers 244; then
        expect "WEAPON-UPGRADE button frame=244 actor=ElvenRivendellKnightHorde slot=5 name=Command_PurchaseUpgradeArnorFireArrows upgrade=Upgrade_ArnorFireArrows state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE result frame=244 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet rank=1 banner=0 button=0 queued=1 complete=0" || return
    fi
    if covers 251; then
        expect "WEAPON-UPGRADE result frame=251 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet rank=1 banner=0 button=1 queued=1 complete=0" || return
    fi
    if covers 540; then
        expect "WEAPON-UPGRADE result frame=540 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet rank=1 banner=0 button=0 queued=0 complete=1" || return
    fi
    if covers 855; then
        expect "WEAPON-UPGRADE result frame=855 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet rank=1 banner=0 button=1 queued=0 complete=1" || return
    fi
    if covers 1168; then
        expect "WEAPON-UPGRADE result frame=1168 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet_Lvl2 rank=2 banner=1 button=0 queued=0 complete=1" || return
    fi
    if covers 1173; then
        expect "WEAPON-UPGRADE result frame=1173 actor=ElvenRivendellKnightHorde set=RivendellLancerHordeCommandSet_Lvl2 rank=2 banner=1 button=1 queued=0 complete=1" || return
    fi
    if covers 1178; then
        expect "WEAPON-UPGRADE result frame=1178 actor=ElvenRivendellKnightHorde set=RivendellLancerBowCommandSet_Lvl2 rank=2 banner=1 button=1 queued=0 complete=1" || return
    fi
    if covers 1195; then
        expect "WEAPON-UPGRADE button frame=1195 actor=GaladhrimWarriorHorde slot=5 name=Command_PurchaseUpgradeElvenSilverthornArrows upgrade=Upgrade_ElvenSilverthornArrows state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE result frame=1195 actor=GaladhrimWarriorHorde set=GalWarriorHordeCommandSet rank=1 banner=0 button=0 queued=0 complete=0" || return
    fi
    if covers 1200; then
        expect "WEAPON-UPGRADE button frame=1200 actor=GaladhrimWarriorHorde slot=5 name=Command_PurchaseUpgradeElvenForgedBlades upgrade=Upgrade_ElvenForgedBlades state=enabled prerequisite=1 affected=1 offered=1 affordable=1" || return
        expect "WEAPON-UPGRADE result frame=1200 actor=GaladhrimWarriorHorde set=GalWarriorHordeSwordCommandSet rank=1 banner=0 button=0 queued=0 complete=0" || return
    fi
}
