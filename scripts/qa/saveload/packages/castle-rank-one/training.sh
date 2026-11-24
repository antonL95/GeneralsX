check() {
    if covers 20; then
        expect 'TEST castle-rank-one 20 construction=0 rank=1 hide=1 banner=0 show=0 wild=0'
    fi
    if covers 33; then
        expect 'FORTRESS order 33 WildFortressCitadel Command_PurchaseUpgradeWildFortressGoblinTown Upgrade_WildFortressGoblinTownTrigger available=1 frames=900'
    fi
    if covers 244; then
        expect 'TEST castle-rank-one 244 construction=0 rank=1 hide=1 banner=0 show=0 wild=0'
    fi
    if covers 942; then
        expect 'TEST castle-rank-one 942 construction=0 rank=1 hide=1 banner=1 show=1 wild=1'
    fi
}
