prepare() {
    [ "${MALLORN_FIXTURE:-}" = 1 ] || return 0
    if [ "$save_frame" -ne 180 ] || [ "$end" -lt 1800 ]; then
        printf '%s\n' 'Mallorn fixture requires save frame 180 and an end frame of at least 1800.' >&2
        return 1
    fi
    printf '%s\n' '29 fund Player_2 ElvenFortress 0'
    awk '
        {
            id = $1; source = $2; upgrade = $3; x = $5; y = $6; angle = $7; path = $8
            if (path == "plot") {
                print 31, "mallorn-construct Player_2 ElvenFortressExpansionPadCorner", plotOrdinal++, source, x, y
            } else {
                print 30, "create Player_2", source, id, x "," y "," angle
                button = "Command_UpgradeMallornTree" upgrade
                if (upgrade == "Niphredil") button = "Command_UpgradeMallornNiphredilBloom"
                if (path == "wall") button = "Command_" upgrade "WallUpgradeToHub"
                print 32, "mallorn-order Player_2", source, id, button, x, y
            }
            print 180, "mallorn-state Player_2", source, id, "-", x, y
            if (path == "plot") print 475, "mallorn-state Player_2", source, id, "Upgrade_MallornTree" upgrade, x, y
            print 1800, "mallorn-state Player_2", source, id, "-", x, y
        }
    ' "$package_dir/cases.tsv"
}

check() {
    [ "${MALLORN_FIXTURE:-}" = 1 ] || return 0
    awk -v first="$first" '
        FNR == NR {
            source[$1] = $2; upgrade[$1] = $3; target[$1] = $4
            x[$1] = $5; y[$1] = $6; angle[$1] = $7; path[$1] = $8
            next
        }
        $1 == "MALLORN_BUILD" { built[$2] = ($3 > 0 && $4 > 0 && $5 == 1) }
        $1 == "MALLORN_ORDER" { ordered[$2] = ($4 > 0 && $5 == 1) }
        $1 == "MALLORN_STATE" {
            frame = $2; caseId = $3; template = $4
            if (template == "ElvenFortressExpansionPadCorner") {
                occupant[frame,caseId] = $10
                castle[frame,caseId] = $11
                keep[frame,caseId] = $12
                hidden[frame,caseId] = $13
                plot[frame,caseId] = $5
            } else {
                count[frame,caseId]++
                if (template == source[caseId]) {
                    sourceId[frame,caseId] = $5
                    building[frame,caseId] = $13
                    queued[frame,caseId] = $14
                }
                if (frame == 1800 && template == target[caseId] && near($6, x[caseId]) && near($7, y[caseId])) {
                    result[caseId] = $5
                    resultPlot[caseId] = $9
                    resultCastle[caseId] = $11
                    resultKeep[caseId] = $12
                }
                if (path[caseId] == "wall" && frame == 1800) {
                    for (i = 1; i <= 3; i++) {
                        offset = (2-i) * 75
                        wx = x[caseId] - (angle[caseId] == 90 ? offset : 0)
                        wy = y[caseId] + (angle[caseId] == 0 ? offset : 0)
                        type = target[caseId] (i == 2 ? "Hub" : "Segment")
                        facing = angle[caseId] == 90 ? 1.571 : 0
                        if (template == type && near($6, wx) && near($7, wy) && near($8, facing))
                            wall[caseId,i] = $5
                    }
                }
            }
        }
        function near(a, b) { return (a-b)^2 < 0.01 }
        function require(ok, message) {
            if (!ok) {
                print "Mallorn case " id " (" source[id] ", " upgrade[id] "): " message
                failed[id] = 1
            }
        }
        END {
            for (id = 1; id <= 14; id++) {
                if (first <= 32)
                    require(path[id] == "plot" ? built[source[id]] : ordered[id], "initial order rejected")
                if (first <= 180 && path[id] == "plot")
                    require(building[180,id] && sourceId[180,id] == occupant[180,id] && hidden[180,id] &&
                        castle[180,id] > 0 && keep[180,id] > 0, "expected occupied, linked plot during construction")
                if (first <= 475 && path[id] == "plot")
                    require(queued[475,id] == "Upgrade_MallornTree" upgrade[id], "authored automatic upgrade missing")
                if (path[id] == "wall") {
                    require(count[1800,id] == 3 && wall[id,1] > 0 && wall[id,1] < wall[id,2] &&
                        wall[id,2] < wall[id,3], "expected ordered segment, hub, segment at authored offsets")
                } else {
                    require(count[1800,id] == 1 && result[id] > 0, "expected only the replacement at the source position")
                    if (path[id] == "plot")
                        require(occupant[1800,id] == result[id] && hidden[1800,id] && castle[1800,id] > 0 &&
                            plot[1800,id] == resultPlot[id] && castle[1800,id] == resultCastle[id] &&
                            keep[1800,id] == resultKeep[id], "replacement lost its plot or castle membership")
                    else
                        require(resultPlot[id] == 0 && resultCastle[id] == 0, "free standing replacement acquired a plot")
                }
                if (!(id in failed))
                    print "MALLORN PASS " id " " source[id] " " upgrade[id]
            }
            exit length(failed) > 0
        }
    ' "$package_dir/cases.tsv" "$log"
}
