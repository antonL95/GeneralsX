check() {
    if covers 241; then
        expect "TEST timed-model-upgrade frame=241 case=checkpoint user2=0"
    fi
    for frame in 242 244 271 272; do
        if covers "$frame"; then
            expect "TEST timed-model-upgrade frame=$frame case=checkpoint user2=1"
        fi
    done
    for frame in 249 252; do
        if covers "$frame"; then
            expect "TEST timed-model-upgrade frame=$frame case=authored user2=0"
        fi
    done
    for frame in 250 251; do
        if covers "$frame"; then
            expect "TEST timed-model-upgrade frame=$frame case=authored user2=1"
        fi
    done
    if covers 273; then
        expect "TEST timed-model-upgrade frame=273 case=checkpoint user2=0"
    fi
    if covers 282; then
        for hit in fire logical-fire; do
            expect "TEST burning-death-fx frame=282 case=$hit authored=1 flames=1 offset=0,0,10"
        done
        for hit in unlit aflame burned-death-type; do
            expect "TEST burning-death-fx frame=282 case=$hit authored=1 flames=0 offset=0,0,0"
        done
    fi
}
