check_line() {
    if covers "$1"; then
        expect "PKG32 $1 $2"
    fi
}

check() {
    check_line 224 'Pkg32Probe model USER_4 1' || return 1
    check_line 224 'Pkg32Probe status UNSELECTABLE 1' || return 1
    check_line 224 'Pkg32Probe weapon-name Pkg32BaseWeapon 1' || return 1
    check_line 224 'Pkg32Probe weapon WEAPONSET_TOGGLE_1 0' || return 1
    check_line 224 'Pkg32BattleTower model USER_11 0' || return 1
    check_line 224 'Pkg32SiegeWorks model USER_2 0' || return 1
    check_line 224 'Pkg32Skeleton model JUST_BUILT 0' || return 1
    check_line 225 'Pkg32Probe model CREATE_A_HERO_01 1' || return 1
    check_line 226 'Pkg32Probe model CREATE_A_HERO_02 1' || return 1
    check_line 226 'Pkg32Probe model USER_15 1' || return 1
    check_line 226 'Pkg32Probe model CREATE_A_HERO_00 0' || return 1
    check_line 226 'Pkg32Probe model CREATE_A_HERO_01 0' || return 1
    check_line 226 'Pkg32Probe model CREATE_A_HERO_03 0' || return 1
    check_line 226 'Pkg32Probe object-upgrade Pkg32Old 0' || return 1
    check_line 226 'Pkg32Probe object-upgrade Pkg32Choice 1' || return 1
    check_line 226 'Pkg32Probe object-upgrade Pkg32Bystander 1' || return 1
    check_line 227 'Pkg32Probe object-upgrade Pkg32Bystander 0' || return 1
    check_line 227 'Pkg32BystanderObject object-upgrade Pkg32Bystander 1' || return 1
    check_line 228 'Pkg32Probe model USER_8 1' || return 1
    check_line 230 'Pkg32Probe model USER_8 0' || return 1
    check_line 231 'Pkg32Probe model USER_5 1' || return 1
    check_line 231 'Pkg32Probe weapon-name Pkg32AlternateWeapon 1' || return 1
    check_line 231 'Pkg32Probe weapon PLAYER_UPGRADE 0' || return 1
    check_line 232 'Pkg32BattleTower model USER_10 1' || return 1
    check_line 232 'Pkg32BattleTower model USER_11 1' || return 1
    check_line 233 'Pkg32SiegeWorks model USER_2 1' || return 1
    check_line 233 'Pkg32SiegeWorks model UPGRADE_IVORY_TOWER 1' || return 1
    check_line 234 'Pkg32Skeleton model JUST_BUILT 1' || return 1
    check_line 240 'Pkg32Probe model USER_16 1' || return 1
    check_line 240 'Pkg32Probe model USER_5 1' || return 1
    check_line 246 'Pkg32Probe model USER_5 1' || return 1
    check_line 246 'Pkg32Probe model USER_16 1' || return 1
    check_line 246 'Pkg32Probe model USER_9 1' || return 1
    check_line 246 'Pkg32BattleTower model USER_11 1' || return 1
    check_line 246 'Pkg32SiegeWorks model USER_2 1' || return 1
    check_line 246 'Pkg32Skeleton model JUST_BUILT 1' || return 1
    check_line 246 'Pkg32BattleTower drawable USER_11 1' || return 1
    check_line 246 'Pkg32SiegeWorks drawable USER_2 1' || return 1
    check_line 246 'Pkg32Skeleton drawable JUST_BUILT 1' || return 1
    check_line 251 'Pkg32SiegeWorks model USER_3 1' || return 1
    check_line 251 'Pkg32SiegeWorks model UPGRADE_HOUSE_OF_HEALING 1' || return 1
    check_line 254 'Pkg32Mine model USER_4 0' || return 1
    check_line 254 'Pkg32Trebuchet model USER_2 0' || return 1
    check_line 256 'Pkg32Mine model USER_4 1' || return 1
    check_line 256 'Pkg32Trebuchet model USER_2 1' || return 1
    check_line 256 'Pkg32Trebuchet weapon PLAYER_UPGRADE 1' || return 1
    check_line 260 'Pkg32Probe model USER_5 1' || return 1
    check_line 262 'Pkg32Probe model USER_9 1' || return 1
    check_line 262 'Pkg32Probe model USER_5 0' || return 1
    check_line 299 'Pkg32Probe model USER_9 1' || return 1
    check_line 301 'Pkg32Probe model USER_9 0' || return 1
    check_line 302 'Pkg32Probe model USER_16 1' || return 1
    check_line 304 'Pkg32Probe model USER_16 0' || return 1
    check_line 305 'Pkg32Probe weapon-name Pkg32BaseWeapon 1' || return 1
    check_line 306 'Pkg32Probe model USER_5 1' || return 1
    check_line 306 'Pkg32Probe weapon WEAPONSET_TOGGLE_1 1' || return 1
    check_line 307 'Player_1 player-upgrade Pkg32Player 0' || return 1
    check_line 307 'Pkg32Probe model USER_4 0' || return 1
    check_line 307 'Pkg32Probe model USER_14 0' || return 1
    check_line 307 'Pkg32Probe status UNSELECTABLE 0' || return 1
    check_line 307 'Pkg32BystanderObject object-upgrade Pkg32Bystander 0' || return 1
    check_line 312 'Pkg32Hero model CREATE_A_HERO_39 1' || return 1
    check_line 312 'Pkg32Hero model USER_27 0' || return 1
    check_line 312 'Pkg32Hero object-upgrade Upgrade_CHW01 1' || return 1
    check_line 312 'Pkg32Rangers weapon WEAPONSET_TOGGLE_2 0' || return 1
    check_line 314 'Pkg32Hero model CREATE_A_HERO_39 0' || return 1
    check_line 314 'Pkg32Hero model CREATE_A_HERO_40 1' || return 1
    check_line 314 'Pkg32Hero model USER_27 1' || return 1
    check_line 314 'Pkg32Hero object-upgrade Upgrade_CHW01 0' || return 1
    check_line 314 'Pkg32Hero object-upgrade Upgrade_CHW03 1' || return 1
    check_line 314 'Pkg32Rangers weapon WEAPONSET_TOGGLE_2 1' || return 1
    check_line 342 'Pkg32Probe model USER_16 0' || return 1
    check_line 353 'Pkg32Skeleton model JUST_BUILT 1' || return 1
    check_line 355 'Pkg32Skeleton model JUST_BUILT 0' || return 1
    check_line 441 'Pkg32BattleTower model USER_11 1' || return 1
    check_line 443 'Pkg32BattleTower model USER_10 1' || return 1
    check_line 443 'Pkg32BattleTower model USER_11 0' || return 1
    check_line 443 'Pkg32BattleTower drawable USER_11 0' || return 1
    check_line 532 'Pkg32SiegeWorks model USER_2 1' || return 1
    check_line 534 'Pkg32SiegeWorks model USER_3 1' || return 1
    check_line 534 'Pkg32SiegeWorks model USER_2 0' || return 1
    check_line 550 'Pkg32SiegeWorks model USER_3 1' || return 1
    check_line 552 'Pkg32SiegeWorks model UPGRADE_IVORY_TOWER 1' || return 1
    check_line 552 'Pkg32SiegeWorks model UPGRADE_HOUSE_OF_HEALING 1' || return 1
    check_line 552 'Pkg32SiegeWorks model USER_3 0' || return 1
    check_line 552 'Pkg32SiegeWorks drawable USER_3 0' || return 1
    return 0
}
