prepare() {
    manifest="$out/lua-creation-manifest.tsv"
    if [ -n "${LUA_CREATION_SWEEP:-}" ]; then
        cp "$LUA_CREATION_SWEEP" "$manifest"
        awk -v first="$((save_frame + 10))" -v end="$end" -v path="$manifest" 'BEGIN {
            for (frame = first; frame <= end && frame < first + 900; frame++)
                print frame, "lua-creation-bulk Player_1 -", frame - first, path, "3310 2900"
        }'
        return
    fi
    awk 'BEGIN { OFS="\t" }
        { print $1, $2, $3, $4, $5 }
    ' > "$manifest" <<'ROWS'
FaramirFunctions OnCreated OnFaramirCreated hide BODY
FaramirFunctions OnCreated OnFaramirCreated hide BODYDUSTER
FaramirFunctions OnCreated OnFaramirCreated upgrade Upgrade_AmrothKnights
FortressFunctions OnCreated OnFortressCreated geometry HighTowerGeom
GuldurFountFunctions Garrisoned OnGuldurFountGarrisoned upgrade Upgrade_MiniHordeLvl6
GuldurFountFunctions Garrisoned OnGuldurFountGarrisoned upgrade Upgrade_MiniHordeLvl7
DormitoryFunctions OnBuildVariation OnDormitoryBuildVariation geometry VersionOne
DormitoryFunctions OnBuildVariation OnDormitoryBuildVariation geometry VersionTwo
VariagOutriderFunctions OnCreated OnVariagOutriderCreated temporary arrownock
LastAllianceTowerFunctions OnBuildingComplete OnLastAllianceTowerCreated upgrade Upgrade_TrebuchetTurret
ROWS
    printf '%s\n' \
        "231 lua-creation-bulk Player_1 GondorFaramir 0 $manifest 3310 2900" \
        "241 create Player_1 RhunVariagHorseArcher 0 3470,2960,0" \
        "243 lua-creation-bulk Player_1 RhunVariagHorseArcher 0 snapshot:$manifest 3470 2960" \
        "245 lua-creation-bulk Player_1 RhunVariagHorseArcher 0 snapshot:$manifest 3470 2960" \
        "258 lua-creation-bulk Player_1 AngmarFortress 0 $manifest 3310 2900" \
        "259 lua-creation-bulk Player_1 ArnorGarrisonTowerExpansion 0 $manifest 3310 2900" \
        "260 lua-creation-bulk Player_1 GuldurFountOfCorruption 0 $manifest 3310 2900" \
        "261 lua-creation-bulk Player_1 RhunVariagHorseArcher 0 $manifest 3310 2900" \
        "270 lua-creation-bulk Player_1 LastAllianceTower 0 $manifest 3310 2900"
}

check() {
    if [ -n "${LUA_CREATION_SWEEP:-}" ]; then
        return
    fi
    if covers 231; then
        grep -Ex 'TEST lua creation sample template=GondorFaramir trial=0 event=OnCreated state=(01|10)1' "$log" || return 1
    fi
    for frame in 243 245; do
        if covers "$frame"; then
            expect "TEST lua creation saved template=RhunVariagHorseArcher frame=$frame state=1" || return 1
        fi
    done
    if covers 258; then
        expect 'TEST lua creation sample template=AngmarFortress trial=0 event=OnCreated state=0' || return 1
    fi
    if covers 259; then
        expect 'TEST lua creation sample template=ArnorGarrisonTowerExpansion trial=1 event=OnBuildVariation state=10' || return 1
        expect 'TEST lua creation sample template=ArnorGarrisonTowerExpansion trial=2 event=OnBuildVariation state=01' || return 1
    fi
    if covers 260; then
        expect 'TEST lua creation sample template=GuldurFountOfCorruption trial=0 event=Garrisoned state=01' || return 1
    fi
    if covers 261; then
        expect 'TEST lua creation sample template=RhunVariagHorseArcher trial=0 event=OnCreated state=1' || return 1
    fi
    if covers 270; then
        expect 'TEST lua creation sample template=LastAllianceTower trial=3 event=OnBuildingComplete state=1' || return 1
    fi
}

compare() {
    awk '/^LUA_CREATION_/' "$out/continue.log" > "$out/lua-creation-observed.tsv"
    if [ -n "${LUA_CREATION_SWEEP:-}" ]; then
        grep -E '^LUA_CREATION_COMPLETE[[:space:]]+[0-9]+$' "$out/lua-creation-observed.tsv"
        awk '/^LUA_CREATION_/' "$out/resume.log" > "$out/lua-creation-resume.tsv"
        cmp "$out/lua-creation-observed.tsv" "$out/lua-creation-resume.tsv"
    fi
}
