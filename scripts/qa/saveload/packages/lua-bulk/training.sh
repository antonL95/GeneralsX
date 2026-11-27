check() {
    if covers 235; then
        awk -F '\t' '$1 == "LUA_BULK" && $2 == "FaramirFunctions" && $3 == "OnCreated" && $4 == "OnFaramirCreated" && $7 == "ran" && $9 == "effect verified" { found=1; print } END { exit !found }' "$log" || return 1
    fi
    if covers 246; then
        awk -F '\t' '$1 == "LUA_BULK" && $2 == "RohanBodyguardFunctions" && $3 == "ModelConditionMOUNTED" && $4 == "OnMountedBodyguard" && $7 == "ran" && $9 == "effect verified" { found=1; print } END { exit !found }' "$log" || return 1
    fi
    if covers 247; then
        awk -F '\t' '$1 == "LUA_BULK" && $2 == "LorienWarriorTDFunctions" && $3 == "BeTowerUpgraded" && $4 == "TDArcherTowerUpgrade01" && $7 == "ran" && $9 == "effect verified" { found=1; print } END { exit !found }' "$log" || return 1
    fi
    if covers 249; then
        awk -F '\t' '$1 == "LUA_BULK" && $2 == "ThreeTrollsFunctions" && $3 == "ReallyDamaged" && $4 == "OnRetreat" && $7 == "ran" && $10 == "sample" { found=1; print } END { exit !found }' "$log" || return 1
    fi
}

prepare() {
    if [ "${LUA_BULK_SWEEP:-0}" = 1 ]; then
        awk -v first="$((save_frame + 8))" -v end="$end" 'BEGIN {
            for (frame = first; frame <= end && frame < first + 360; frame++)
                print frame, "lua-bulk Player_1 -", frame - first, "sweep 3310 2900"
        }'
    fi
}

compare() {
    if [ "${LUA_BULK_SWEEP:-0}" = 1 ]; then
        awk -F '\t' '$1 == "LUA_BULK" || $0 ~ /^TEST lua bulk sweep pairs=/ { print }' "$out/continue.log" > "$out/lua-bulk.tsv"
        grep -E '^TEST lua bulk sweep pairs=[0-9]+$' "$out/continue.log"
    fi
}
