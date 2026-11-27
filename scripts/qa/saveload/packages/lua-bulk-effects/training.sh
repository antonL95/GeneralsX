expect_effect() {
    if covers "$1"; then
        awk -F '\t' -v handler="$2" '$1 == "LUA_BULK" && $4 == handler && $7 == "ran" && $9 == "effect verified" && $10 == "sample" { found=1; print } END { exit !found }' "$log"
    fi
}

check() {
    expect_effect 1100 OnNecromancerGetTheRing || return 1
    expect_effect 1101 OnLibraryIdle || return 1
    expect_effect 1103 OnTwinsDeathEggCreated || return 1
    expect_effect 1104 DragonStrikeDragonCreated || return 1
    if covers 1108; then expect 'TEST check-upgrade frame=1108 found=0' || return 1; fi
    if covers 1110; then expect 'TEST check-upgrade frame=1110 found=1' || return 1; fi
    if covers 1111; then expect 'TEST check-upgrade frame=1111 found=0' || return 1; fi
    if covers 1122; then expect 'TEST lua bulk condition frame=1122 player=PlyrCreeps set=0' || return 1; fi
    if covers 1124; then expect 'TEST lua bulk condition frame=1124 player=PlyrCreeps set=0' || return 1; fi
    if covers 1125; then expect 'TEST lua bulk condition frame=1125 player=PlyrCreeps set=1' || return 1; fi
    if covers 1126; then expect 'TEST lua bulk condition frame=1126 player=Player_1 set=0' || return 1; fi
    if covers 1130; then
        awk -F '\t' '$1 == "LUA_BULK" && $2 == "BaseScriptFunctions" && $3 == "ReallyDamaged" && $4 == "OnRetreat" && $6 == "LuaBulkForcedRegistration" && $7 == "ran" && $9 == "effect verified" { found=1; print } END { exit !found }' "$log" || return 1
    fi
}
