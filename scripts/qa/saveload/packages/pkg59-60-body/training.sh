check_burning() {
    if covers 240; then
        expect "TEST pkg59 frame=240 label=wolf-fire hp=1 dead=0 burning=318 model=1"
        expect "TEST pkg59 frame=240 label=wolf-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=camel-fire hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=camel-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=cat-fire hp=1 dead=0 burning=318 model=1"
        expect "TEST pkg59 frame=240 label=cat-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=variag-fire hp=1 dead=0 burning=390 model=1"
        expect "TEST pkg59 frame=240 label=variag-control hp=0 dead=1 burning=0 model=0"
        awk '
            $1 == "TEST" && $2 == "pkg59" && $3 == "credit" && $4 == "frame=240" {
                split($7, kills, "=")
                if ($5 == "label=before" && $6 == "xp=1") {
                    before = kills[2]
                    baseline = 1
                }
                if ($5 == "label=burning" && $6 == "xp=7")
                    awarded = baseline && kills[2] == before + 1
            }
            END { exit !awarded }
        ' "$log"
        grep -Eq '^TEST pkg59 consumers frame=240 label=before alive=1 selectable=1 attack=[01] target=1 acquire=1 fraction=1.000000$' "$log"
        awk '
            $1 == "TEST" && $2 == "pkg59" && $3 == "damage" && $4 == "frame=240" && $5 == "label=variag-fire" {
                split($6, clipped, "=")
                split($7, maximum, "=")
                found = clipped[2] == maximum[2] - 1
            }
            END { exit !found }
        ' "$log"
        for actor in wolf cat; do
            expect "TEST pkg59 FX frame=240 label=$actor-fire choice=cavalry flames=1 offset=0,0,10"
            expect "TEST pkg59 FX frame=240 label=$actor-control choice=cavalry flames=0 offset=0,0,0"
        done
        for actor in camel variag; do
            expect "TEST pkg59 FX frame=240 label=$actor-fire choice=infantry flames=1 offset=0,0,10"
            expect "TEST pkg59 FX frame=240 label=$actor-control choice=infantry flames=0 offset=0,0,0"
        done
    fi
    for row in 240:burning 244:checkpoint; do
        frame=${row%:*}
        label=${row#*:}
        if covers "$frame"; then
            awk -v frame="frame=$frame" -v label="label=$label" '
                $1 == "TEST" && $2 == "pkg59" && $3 == "consumers" && $4 == frame && $5 == label &&
                    $6 == "alive=1" && $7 == "selectable=0" && $8 == "attack=0" &&
                    $9 == "target=0" && $10 == "acquire=0" {
                    split($11, health, "=")
                    found = health[2] > 0 && health[2] < 0.01
                }
                END { exit !found }
            ' "$log"
        fi
    done
    for row in 244:checkpoint 391:dead; do
        frame=${row%:*}
        if covers "$frame"; then
            grep -E "^TEST pkg59 credit frame=$frame label=${row#*:} xp=7 kills=[0-9]+$" "$log"
            if grep -q '^246 pkg59-pickup ' "$out/commands.txt"; then
                expect "TEST pkg59 credit frame=$frame label=${row#*:} xp=7 kills=1"
            fi
        fi
    done
    if covers 245; then
        expect "TEST pkg59 frame=245 label=wolf-fire hp=1 dead=0 burning=318 model=1"
        expect "TEST pkg59 frame=245 label=cat-fire hp=1 dead=0 burning=318 model=1"
        expect "TEST pkg59 frame=245 label=variag-fire hp=1 dead=0 burning=390 model=1"
        expect 'TEST pkg59 healing frame=245 damage=0 before=1 after=1'
    fi
    if covers 246 && grep -q '^246 pkg59-pickup ' "$out/commands.txt"; then
        expect 'TEST pkg59 pickup frame=246 collected=1 hp=1'
    fi
    for actor in wolf cat; do
        if covers 317; then
            expect "TEST pkg59 frame=317 label=$actor-fire hp=1 dead=0 burning=318 model=1"
        fi
        if covers 318; then
            expect "TEST pkg59 frame=318 label=$actor-fire hp=1 dead=0 burning=318 model=1"
        fi
        if covers 319; then
            expect "TEST pkg59 frame=319 label=$actor-fire hp=0 dead=1 burning=0 model=0"
            expect "TEST pkg59 death frame=319 label=$actor-fire hit=318"
        fi
    done
    if covers 389; then
        expect "TEST pkg59 frame=389 label=variag-fire hp=1 dead=0 burning=390 model=1"
    fi
    if covers 390; then
        expect "TEST pkg59 frame=390 label=variag-fire hp=1 dead=0 burning=390 model=1"
    fi
    if covers 391; then
        expect "TEST pkg59 frame=391 label=variag-fire hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 death frame=391 label=variag-fire hit=390"
        expect 'TEST pkg59 consumers frame=391 label=dead alive=0 selectable=0 attack=0 target=0 acquire=0 fraction=0.000000'
    fi
    for pair in camel-fire:481 camel-control:481 cat-control:481 cat-fire:559 variag-control:541 variag-fire:691 wolf-control:751 wolf-fire:829; do
        frame=${pair#*:}
        before=$((frame - 2))
        if covers "$before"; then
            expect "TEST pkg59 frame=$before label=${pair%:*} hp=0 dead=1 burning=0 model=0"
        fi
        if covers "$frame"; then
            expect "TEST pkg59 frame=$frame label=${pair%:*} gone"
        fi
    done
}

check_reactions() {
    if covers 250; then
        if [ "$phase" = resume ]; then
            expect 'TEST pkg60 recovery frame=250 duration=0'
        else
            expect 'TEST pkg60 recovery frame=250 duration=150'
        fi
    fi
    for row in 251:500:0 252:240:1 279:240:1 280:430:0 402:430:0; do
        frame=${row%%:*}
        rest=${row#*:}
        if covers "$frame"; then
            expect "TEST pkg60 state frame=$frame label=recovery hp=${rest%:*} state=${rest#*:} radius=0 cheer=0 front=0"
        fi
    done
    if covers 885; then
        awk '
            $1 == "TEST" && $2 == "pkg60" && $3 == "dodge" {
                split($8, hits, /[=\/]/)
                count[$5, $7] = hits[2]
                print
                if (hits[3] != 64 || $9 != "results=1" || $10 != "stream=1")
                    failed = 1
                if (($5 == "actor=HaradHasharinBlade" && $6 != "chance=0.80") ||
                    ($5 == "actor=HaradZigur" && $6 != "chance=0.70"))
                    failed = 1
            }
            END {
                if (NR == 0)
                    failed = 1
                actors[1] = "actor=HaradHasharinBlade"
                actors[2] = "actor=HaradZigur"
                for (i = 1; i <= 2; ++i) {
                    dodged = count[actors[i], "dodgeable=1"]
                    control = count[actors[i], "dodgeable=0"]
                    if (dodged <= 0 || control <= dodged || control >= 64)
                        failed = 1
                }
                exit failed
            }
        ' "$log"
    fi
    for frame in 904 914 924 934 944; do
        if covers "$frame"; then
            grep -E "^TEST pkg60 state frame=$frame label=radius hp=[0-9]+ state=0 radius=100 cheer=0 front=0$" "$log"
            expect "TEST pkg60 state frame=$frame label=near hp=100 state=0 radius=0 cheer=1 front=0"
            expect "TEST pkg60 state frame=$frame label=far hp=100 state=0 radius=0 cheer=0 front=0"
            expect "TEST pkg60 state frame=$frame label=allied hp=100 state=0 radius=0 cheer=0 front=0"
        fi
    done
    if covers 981; then
        expect 'TEST pkg60 heal frame=981 label=full gain=0 configured=1 effects=0'
        expect 'TEST pkg60 heal frame=981 label=none gain=100 configured=0 effects=0'
        expect 'TEST pkg60 heal frame=981 label=configured gain=100 configured=1 effects=1'
    fi
    if covers 1021; then
        expect 'TEST pkg60 state frame=1021 label=undamaged hp=3000 state=0 radius=0 cheer=0 front=0'
        expect 'TEST pkg60 debris frame=1021 hp=2978 state=0 bits=0 chunks=0 front=0'
    fi
    if covers 1022; then
        expect 'TEST pkg60 debris frame=1022 hp=2318 state=0 bits=7 chunks=1 front=1'
    fi
    if covers 1023; then
        expect 'TEST pkg60 debris frame=1023 hp=1658 state=1 bits=7 chunks=0 front=0'
    fi
    if covers 1024; then
        expect 'TEST pkg60 debris frame=1024 hp=998 state=2 bits=7 chunks=0 front=0'
    fi
    if covers 1025; then
        expect 'TEST pkg60 debris frame=1025 hp=338 state=2 bits=7 chunks=1 front=0'
    fi
}

prepare() {
    printf "%s pkg59-load-levels - - 0 %s/levels.ini\n" 238 "$package_dir" "$save_frame" "$package_dir"
}

check() {
    check_burning
    check_reactions
}
