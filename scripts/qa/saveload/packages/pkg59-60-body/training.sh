check() {
    if covers 240; then
        expect "TEST pkg59 frame=240 label=wolf-fire hp=0 dead=1 burning=318 model=1"
        expect "TEST pkg59 frame=240 label=wolf-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=camel-fire hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=camel-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=cat-fire hp=0 dead=1 burning=318 model=1"
        expect "TEST pkg59 frame=240 label=cat-control hp=0 dead=1 burning=0 model=0"
        expect "TEST pkg59 frame=240 label=variag-fire hp=0 dead=1 burning=390 model=1"
        expect "TEST pkg59 frame=240 label=variag-control hp=0 dead=1 burning=0 model=0"
        for actor in wolf cat; do
            expect "TEST pkg59 FX frame=240 label=$actor-fire choice=cavalry flames=1 offset=0,0,10"
            expect "TEST pkg59 FX frame=240 label=$actor-control choice=cavalry flames=0 offset=0,0,0"
        done
        for actor in camel variag; do
            expect "TEST pkg59 FX frame=240 label=$actor-fire choice=infantry flames=1 offset=0,0,10"
            expect "TEST pkg59 FX frame=240 label=$actor-control choice=infantry flames=0 offset=0,0,0"
        done
    fi
    if covers 245; then
        expect "TEST pkg59 frame=245 label=wolf-fire hp=0 dead=1 burning=318 model=1"
        expect "TEST pkg59 frame=245 label=cat-fire hp=0 dead=1 burning=318 model=1"
        expect "TEST pkg59 frame=245 label=variag-fire hp=0 dead=1 burning=390 model=1"
    fi
    for actor in wolf cat; do
        if covers 317; then
            expect "TEST pkg59 frame=317 label=$actor-fire hp=0 dead=1 burning=318 model=1"
        fi
        if covers 319; then
            expect "TEST pkg59 frame=319 label=$actor-fire hp=0 dead=1 burning=0 model=0"
        fi
    done
    if covers 389; then
        expect "TEST pkg59 frame=389 label=variag-fire hp=0 dead=1 burning=390 model=1"
    fi
    if covers 391; then
        expect "TEST pkg59 frame=391 label=variag-fire hp=0 dead=1 burning=0 model=0"
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
