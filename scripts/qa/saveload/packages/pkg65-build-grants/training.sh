grants='AngmarWall Upgrade_TrebuchetTurret
HaradExpansion Upgrade_TrebuchetTurret
Sanctum Upgrade_AngmarHallofTwilightLevel1
Sanctum Upgrade_MiniHordeLvl4
Sanctum Upgrade_MiniHordeLvl10
Umbar Upgrade_MiniHordeLvl10
Pavillion Upgrade_MiniHordeLvl10
Mahud Upgrade_MiniHordeLvl10
Bazaar Upgrade_MiniHordeLvl10
Harondor Upgrade_MiniHordeLvl10'

payloads='AngmarWall HaradTrebuchetFortress 1
HaradExpansion HaradTrebuchetFortress 1
Sanctum HaradZigurCultistAcolyte_Slaved 3
Umbar HaradBlackNumenoreanWarrior_Balcony 1
Pavillion PavillionIdleVulture 1
Pavillion PavillionIdleVulture2 1
Pavillion PavillionIdleVulture3 1
Mahud HaradMahudWarrior_Sitting 2
Bazaar HaradTrader_DeployedCaravan 1'

alone() {
    grep -q 'create Player_1 Pkg65SiegePen' "$out/commands.txt"
}

prepare() {
    alone || return 0
    printf '%s\n' "$grants" | while read -r name upgrade; do
        for frame in 243 245 255 295 296 297 298 299 300 301 302 303 304 305 306 310 450; do
            printf '%s pkg65-grant Player_1 Pkg65%s 0 %s\n' "$frame" "$name" "$upgrade"
        done
        for frame in 243 245 255; do
            printf '%s pkg65-grant Player_1 Pkg65%s 1 %s\n' "$frame" "$name" "$upgrade"
        done
    done
    printf '%s\n' "$payloads" | while read -r name child count; do
        for frame in 243 245 310 450; do
            printf '%s pkg65-children Player_1 Pkg65%s 0 %s\n' "$frame" "$name" "$child"
        done
    done
    for frame in $(seq 285 390); do
        printf '%s pkg65-grant Player_1 Pkg65DozerBazaar 0 Upgrade_MiniHordeLvl10\n' "$frame"
    done
    for frame in $(seq 365 410); do
        printf '%s pkg65-grant Player_1 Pkg65SiegeMumak 0 Upgrade_SwitchToRockThrowing\n' "$frame"
    done
    for tech in HeavyArmor ForgedBlades FireArrows; do
        for frame in $(seq 253 299); do
            printf '%s pkg65-grant Player_1 HaradPatrolUpgrateGranter 0 Upgrade_TechnologyGondor%s\n' "$frame" "$tech"
        done
    done
    printf '%s\n' '450 pkg65-children Player_1 Pkg65Harondor 0 HaradWarlord_Slaved'
}

probe() {
    if covers "$1" && ! grep -E "^TEST pkg65 grant frame=$1 object=$2 index=$3 upgrade=$4 $5" "$log"; then
        printf '%s frame %s %s ordinal %s expected %s %s\n' "$phase" "$1" "$2" "$3" "$4" "$5" >&2
        return 1
    fi
}

children() {
    if covers "$1"; then
        expect "TEST pkg65 children frame=$1 object=$2 child=$3 count=$4"
    fi
}

completion_frame() {
    covers "$3" && covers "$4" || return 0
    awk -v object="$1" -v upgrade="$2" -v begin="$3" -v end="$4" -v mode="${5:-construction}" '
        $2 == "pkg65" && $3 == "grant" && $5 == "object=" object &&
        $6 == "index=0" && $7 == "upgrade=" upgrade {
            split($4, stamp, "=")
            frame = stamp[2] + 0
            if (frame < begin || frame > end)
                next
            present = ($8 == "present=1")
            building = ($9 == "building=1")
            granted = ($11 == "granted=1")
            if (mode != "construction" && present != granted)
                bad = 1
            if (mode == "weapon" && present && $13 != "toggle=1")
                bad = 1
            if (mode == "construction" && (!present || building == granted))
                bad = 1
            if (granted && !firstGrant)
                firstGrant = frame
            if (!granted)
                lastPending = frame
        }
        END {
            if (bad || !firstGrant || firstGrant != lastPending + 1) {
                print object, upgrade, "completion transition missing or inconsistent"
                exit 1
            }
            print object, upgrade, "first granted at frame", firstGrant
        }
    ' "$log"
}

check() {
    probe 240 Pkg65OclGrant 0 Upgrade_MiniHordeLvl10 'present=1 building=0 pending=0 granted=1 rider5=1' || return 1
    probe 245 Pkg65OclGrant 0 Upgrade_MiniHordeLvl10 'present=1 building=0 pending=0 granted=1 rider5=1' || return 1
    alone || return 0
    printf '%s\n' "$grants" | while read -r name upgrade; do
        for frame in 243 245; do
            for index in 0 1; do
                probe "$frame" "Pkg65$name" "$index" "$upgrade" 'present=1 building=1 pending=[1-9][0-9]* granted=0' || exit 1
            done
        done
        probe 255 "Pkg65$name" 1 "$upgrade" 'present=0 building=0 pending=0 granted=0' || exit 1
        probe 310 "Pkg65$name" 0 "$upgrade" 'present=1 building=0 pending=0 granted=1' || exit 1
        probe 450 "Pkg65$name" 0 "$upgrade" 'present=1 building=0 pending=0 granted=1' || exit 1
        completion_frame "Pkg65$name" "$upgrade" 295 310 || exit 1
    done || return 1
    printf '%s\n' "$payloads" | while read -r name child count; do
        children 243 "Pkg65$name" "$child" 0 || exit 1
        children 245 "Pkg65$name" "$child" 0 || exit 1
        children 450 "Pkg65$name" "$child" "$count" || exit 1
    done || return 1
    probe 310 Pkg65Sanctum 0 Upgrade_MiniHordeLvl4 'present=1 building=0 pending=0 granted=1 rider5=1' || return 1
    for index in 0 1; do
        probe 243 Pkg65DozerBazaar "$index" Upgrade_MiniHordeLvl10 'present=1 building=1 pending=2 granted=0' || return 1
    done
    probe 247 Pkg65DozerBazaar 0 Upgrade_MiniHordeLvl10 'present=1 building=1 pending=2 granted=0' || return 1
    probe 255 Pkg65DozerBazaar 1 Upgrade_MiniHordeLvl10 'present=0 building=0 pending=0 granted=0' || return 1
    probe 390 Pkg65DozerBazaar 0 Upgrade_MiniHordeLvl10 'present=1 building=0 pending=0 granted=1' || return 1
    completion_frame Pkg65DozerBazaar Upgrade_MiniHordeLvl10 285 390 || return 1
    children 450 Pkg65DozerBazaar HaradTrader_DeployedCaravan 1 || return 1
    probe 310 Pkg65Umbar 1 Upgrade_MiniHordeLvl10 'present=1 building=0 pending=0 granted=1' || return 1
    probe 237 WotRHaradCitadel 0 Upgrade_AngmarFortressAngmarStonework 'present=1 building=0 pending=0 granted=1' || return 1
    probe 245 WotRHaradCitadel 0 Upgrade_AngmarFortressAngmarStonework 'present=1 building=0 pending=0 granted=1' || return 1
    probe 320 HaradPorterWotR 0 Upgrade_MenWallPosternGate 'present=1 .*granted=1 .*set=AngmarPorterCommandSetWotR$' || return 1
    children 450 Pkg65Harondor HaradWarlord_Slaved 0 || return 1
    children 1300 Pkg65Harondor HaradWarlord_Slaved 1 || return 1
    for tech in HeavyArmor ForgedBlades FireArrows; do
        probe 252 HaradPatrolUpgrateGranter 0 "Upgrade_TechnologyGondor$tech" 'present=0 building=0 pending=0 granted=0' || return 1
        probe 300 HaradPatrolUpgrateGranter 0 "Upgrade_TechnologyGondor$tech" 'present=1 building=0 pending=0 granted=1' || return 1
        completion_frame HaradPatrolUpgrateGranter "Upgrade_TechnologyGondor$tech" 252 300 release || return 1
    done
    if covers 250; then
        expect 'TEST pkg65 cancel production frame=250 object=Pkg65SiegePen' || return 1
    fi
    probe 253 Pkg65SiegeMumak 0 Upgrade_SwitchToRockThrowing 'present=0 building=0 pending=0 granted=0' || return 1
    probe 365 Pkg65SiegeMumak 0 Upgrade_SwitchToRockThrowing 'present=0 building=0 pending=0 granted=0' || return 1
    probe 490 Pkg65SiegeMumak 0 Upgrade_SwitchToRockThrowing 'present=1 building=0 pending=0 granted=1 .*weapon=SiegeMumakRock ' || return 1
    probe 490 Pkg65SiegeMumak 1 Upgrade_SwitchToRockThrowing 'present=0 building=0 pending=0 granted=0' || return 1
    completion_frame Pkg65SiegeMumak Upgrade_SwitchToRockThrowing 365 410 weapon || return 1
}
