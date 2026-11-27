handler_expect() {
    if covers "$1"; then
        expect "$2"
    fi
}

check() {
    handler_expect 221 'TEST lua handler power frame=221 power=SpecialAbilityCellarDoorSummon ready=1' || return 1
    handler_expect 222 'TEST lua handler power frame=222 power=SpecialAbilityDwarvenMineshaftTunnelCollapse ready=1' || return 1
    handler_expect 222 'TEST lua handler object frame=222 template=DurinROPPing present=0' || return 1
    handler_expect 222 'TEST lua handler object frame=222 template=TreasureChest1000 present=0' || return 1
    handler_expect 225 'TEST lua handler power frame=225 power=SpecialAbilityCellarDoorSummon ready=1' || return 1
    handler_expect 226 'TEST lua handler object frame=226 template=DurinROPPing present=1' || return 1
    handler_expect 226 'TEST lua handler object frame=226 template=TreasureChest1000 present=1' || return 1
    handler_expect 229 'TEST lua handler hit frame=229 owner=PlyrCreeps' || return 1
    handler_expect 261 'TEST lua handler hit frame=261 owner=PlyrCreeps' || return 1
    handler_expect 263 'TEST lua handler object frame=263 template=NeutralGollum_RingStealer present=1' || return 1
    for frame in 232 262; do
        handler_expect "$frame" "TEST lua handler geometry frame=$frame name=HighTowerGeom count=1 active=0" || return 1
    done
    for frame in 233 263; do
        handler_expect "$frame" "TEST lua handler subobject frame=$frame name=DBFBANNER hidden=1" || return 1
    done
    for frame in 237 238; do
        handler_expect "$frame" "TEST lua handler build frame=$frame built=1" || return 1
    done
    for frame in 241 264; do
        handler_expect "$frame" "TEST lua handler geometry frame=$frame name=VersionOne count=1 active=1" || return 1
        handler_expect "$frame" "TEST lua handler geometry frame=$frame name=VersionTwo count=2 active=0" || return 1
    done
    for frame in 242 265; do
        handler_expect "$frame" "TEST lua handler geometry frame=$frame name=VersionOne count=1 active=0" || return 1
        handler_expect "$frame" "TEST lua handler geometry frame=$frame name=VersionTwo count=2 active=2" || return 1
    done
    handler_expect 266 'TEST lua bulk condition frame=266 player=PlyrCreeps set=1' || return 1
    handler_expect 267 'TEST lua bulk condition frame=267 player=Player_1 set=0' || return 1
    handler_expect 268 'TEST lua handler object frame=268 template=DurinROPPing present=1' || return 1
    handler_expect 268 'TEST lua handler power frame=268 power=SpecialAbilityDwarvenMineshaftTunnelCollapse ready=0' || return 1
    handler_expect 287 'TEST lua bulk condition frame=287 player=PlyrCreeps set=0' || return 1
    handler_expect 289 'TEST lua bulk condition frame=289 player=PlyrCreeps set=1' || return 1
    handler_expect 290 'TEST lua handler scripted frame=290 event=BeAfraidOfRampage ran=0' || return 1
    handler_expect 291 'TEST lua bulk condition frame=291 player=Player_1 set=0' || return 1
    if covers 221 && covers 291; then
        printf 'LUA_BULK\t%s\t%s\t%s\t%s\t%s\tran\t\teffect verified\tsample\n' \
            LastAllianceTowerFunctions User31IsActive OnDurinROPActivated model LastAllianceTower \
            NeutralGollum_RingStealerFunctions OnDamaged NeutralGollum_RingStealerDamaged internal NeutralGollum_RingStealer \
            FortressFunctions OnCreated OnFortressCreated internal AngmarFortress \
            DormitoryFunctions OnBuildVariation OnDormitoryBuildVariation internal ArnorGarrisonTowerExpansion
        printf 'LUA_BULK\tCaveTrollFunctions\tReallyDamaged\tGoIntoRampage\tmodel\tGoblinCaveTroll\tran\tBeAfraidOfRampage\tbroadcast has no handler\tsample\n'
    fi
}
