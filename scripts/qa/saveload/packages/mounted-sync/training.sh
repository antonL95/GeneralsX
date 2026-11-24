check() {
    if covers 244 && covers 382; then
        awk '
            $1 == "TEST" && $2 == "mounted-sync" {
                for (i = 5; i <= NF; ++i) {
                    split($i, field, "=")
                    value[field[1]] = field[2]
                }
                frame = $3
                if (value["power"] == "SpecialAbilityHornOfGondor") {
                    if (frame == 244 || frame == 265 || frame == 292 || frame == 310 || frame == 382) {
                        id[frame] = value["id"]
                        position[frame] = value["pos"]
                        hp[frame] = value["hp"]
                        maxhp[frame] = value["maxhp"]
                        deadline[frame] = value["deadline"]
                        selection[frame] = value["selected"]
                        opacity[frame] = value["opacity"] + 0
                        form[frame] = $4
                        if (value["upgrade"] != 1 || value["rank"] != 5 || value["xp"] <= 0 || value["ready"] != 0)
                            failure = "Merry lost his upgrade, experience, rank or Horn cooldown at frame " frame
                        if (frame == 244)
                            xp = value["xp"]
                        else if (value["xp"] != xp)
                            failure = "Merry experience changed at frame " frame
                    }
                }
                if (value["power"] == "SpecialAbilityMerryBlade") {
                    blade[frame] = value["deadline"]
                    bladeReady[frame] = value["ready"]
                }
            }
            END {
                for (frame in form)
                    if (id[frame] != id[244] || position[frame] != position[244] || deadline[frame] != deadline[244])
                        failure = "Merry lost his identity, position or Horn deadline at frame " frame
                if (form[244] != "ShireMeriadoc" || form[265] != "ShireMeriadoc" || form[292] != "ShireMeriadocMounted" ||
                    form[310] != "ShireMeriadocMounted" || form[382] != "ShireMeriadoc" ||
                    deadline[244] <= 382 || hp[244] <= 0 || hp[244] >= 1 || hp[292] != hp[244] || maxhp[292] == maxhp[244] ||
                    hp[382] <= 0 || hp[382] >= 1 || selection[292] != 1 || selection[382] != 1)
                    failure = "Merry mount and dismount state did not match"
                if (!(opacity[244] < 1 && opacity[244] > opacity[265] && opacity[265] > 0 &&
                    opacity[292] > 0 && opacity[292] < 1 && opacity[310] == 1))
                    failure = "Merry did not fade out and back in across the swap"
                if (!(blade[265] > 293 && bladeReady[265] == 0 && blade[293] > 265 &&
                    blade[293] <= 293 && bladeReady[293] == 1))
                    failure = "Merry Blade did not use the new form timer after swapping a paused power"
                if (failure) {
                    print "mounted-sync: " failure
                    exit 1
                }
                print "mounted-sync: two swaps, Horn cooldown, paused timer exclusion and replacement fade passed"
            }
        ' "$log" || return 1
    fi

    if covers 419 && covers 563; then
        expect "TEST frame 419: power SpecialAbilityGlorfindelBladeOfPurity has no button" || return 1
        awk '
            $1 == "TEST" && $2 == "mounted-sync" && $4 == "GondorFaramir" {
                for (i = 5; i <= NF; ++i) {
                    split($i, field, "=")
                    value[field[1]] = field[2]
                }
                opacity[$3] = value["opacity"] + 0
                if (value["power"] == "SpecialAbilityToggleMountedTwo")
                    ready[$3] = value["ready"]
            }
            END {
                if (!(opacity[443] < 1 && opacity[443] > opacity[492] && opacity[492] >= 0.29 &&
                    opacity[492] < opacity[510] && opacity[510] < 1 && opacity[562] == 1)) {
                    print "mounted-sync: Faramir in-place mount did not fade out and back in"
                    exit 1
                }
                if (!(492 in ready) || ready[492] != 0 || ready[563] != 1) {
                    print "mounted-sync: Faramir alternate mount power did not recharge"
                    exit 1
                }
                print "mounted-sync: Faramir in-place fade and alternate recharge passed"
            }
        ' "$log" || return 1
    fi

    for probe in '600 GondorFaramir FaramirDesperateCharge' '690 GondorImrahil ImrahilUnscathed' \
        '740 GondorImrahil ImrahilGloriousCharge' '950 GondorElessar ElessarBladeMaster'; do
        set -- $probe
        if covers "$1"; then
            grep -E "^TEST mounted-sync $1 $2 .*mounted=1 .*ready=0 .*mods=.*$3\\(" "$log" || return 1
        fi
    done
}

compare() {
    for run in continue resume; do
        awk -v first="$save_frame" '
            $1 == "TEST" && $2 == "mounted-sync" && $3 >= first {
                if ($3 < 266)
                    sub(/ selected=[01]/, "")
                print
            }
        ' "$out/$run.log" > "$out/mounted-sync-$run.txt"
    done
    diff -u "$out/mounted-sync-continue.txt" "$out/mounted-sync-resume.txt"
}
