check() {
    if covers 248; then
        expect "TEST map-unit-ops guard from frame=248 area=1 matched=1 fallback=1"
    fi
    if grep -q ' map-unit-ops-force-emotion ' "$out/commands.txt"; then
        for frame in 243 245; do
            if covers "$frame"; then
                expect "TEST map-unit-ops player emotion frame=$frame allies=1 enemy=0"
            fi
        done
    fi
    if covers 247; then
        expect "TEST map-unit-ops guard area frame=247 area=1 matched=1"
    fi
    if covers 243; then
        expect "TEST map-unit-ops team path frame=243 path=1 ordered=1 modes=0"
    fi
    if covers 245; then
        expect "TEST map-unit-ops team path frame=245 path=1 ordered=1 modes=1"
    fi
    for frame in 243 245; do
        if covers "$frame"; then
            expect "TEST map-unit-ops named path frame=$frame path=1 ordered=1 missing=1"
        fi
    done
    for frame in 240 241 242 248; do
        if covers "$frame"; then
            expect "TEST map-unit-ops victim frame=$frame dead=1"
        fi
    done
    for frame in 243 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops kills frame=$frame single=1 list=1 owner=1"
        fi
    done
    for frame in 242 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops threat frame=$frame match=1 reject=1 cached=1"
        fi
    done
    for frame in 243 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops health frame=$frame partial=1 match=1 reject=1"
        fi
    done
    for frame in 240 248; do
        if covers "$frame"; then
            expect "TEST map-unit-ops team-xp frame=$frame sink=1 members=1"
        fi
    done
    for frame in 243 249; do
        if covers "$frame"; then
            expect "TEST map-unit-ops veterancy frame=$frame match=1 reject=1"
        fi
    done
    for frame in 239 248; do
        if covers "$frame"; then
            expect "TEST map-unit-ops xp frame=$frame sink=1 amount=1 negative=1"
        fi
    done
    for frame in 241 247; do
        if covers "$frame"; then
            expect "TEST map-unit-ops teleport frame=$frame moved=1 missing=1"
        fi
    done
    for frame in 240 246; do
        if covers "$frame"; then
            expect "TEST map-unit-ops frame=$frame attack=1 missing=1"
        fi
    done
}
