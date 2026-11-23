check() {
    if covers 228; then
        expect "TEST creep-entry eligibility frame=228 target=CreepEntryTroll player=0 ai=0"
        expect "TEST creep-entry eligibility frame=228 target=NeutralWarg player=0 ai=0"
        expect "TEST creep-entry eligibility frame=228 target=MinorSpider player=0 ai=0"
    fi
    if covers 232; then
        grep -F "TEST creep-entry state frame=232 dead=0 contained=0 target-container=0 player-attacking=0 entering=0 " "$log"
    fi
    if covers 238; then
        expect "TEST creep-entry context frame=238 hover=1 attack=1 enter=0"
        expect "TEST creep-entry context frame=238 hover=0 attack=1 enter=0"
    fi
    for frame in 242 249; do
        if covers "$frame"; then
            grep -F "TEST creep-entry state frame=$frame dead=0 contained=0 target-container=0 player-attacking=1 " "$log"
        fi
    done
    if covers 425; then
        grep -F "TEST creep-entry state frame=425 dead=0 contained=0 target-container=0 " "$log"
        grep -F "TEST creep-entry state frame=425 dead=1 contained=0 target-container=0 " "$log"
    fi
    if covers 451; then
        expect "TEST creep-entry eligibility frame=451 target=CreepEntryKeep player=1 ai=1"
        expect "TEST creep-entry eligibility frame=451 target=CreepEntryGrabber player=0 ai=0"
    fi
    if covers 485; then
        grep -F "TEST creep-entry state frame=485 dead=0 contained=1 target-container=1 " "$log"
    fi
    if covers 600; then
        grep -F "TEST creep-entry state frame=600 dead=0 contained=1 target-container=1 " "$log"
    fi
}
