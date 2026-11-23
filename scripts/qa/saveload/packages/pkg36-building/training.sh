site() {
    grep -E "^TEST pkg36 site frame=$1 name=$2 $3" "$log"
}

health() {
    sed -nE "s/^TEST pkg36 site frame=$1 name=$2 .* health=([.0-9]+) .*/\1/p" "$log"
}

check() {
    if covers 243; then
        site 243 Pkg36Farm 'builder=GondorWorkerNoSelect producer=Pkg36Porter progress=[1-9][0-9]*\.[0-9] under=1 '
        site 243 Pkg36Farm 'builder=Pkg36Porter producer=Pkg36Porter progress=0\.0 under=1 '
    fi
    if covers 320; then
        site 320 Pkg36Farm 'builder=GondorWorkerNoSelect producer=Pkg36Porter progress=[1-9][0-9]*\.[0-9] under=1 '
        site 320 Pkg36Farm 'builder=Pkg36Porter producer=Pkg36Porter progress=0\.0 under=1 '
    fi
    if covers 321; then
        grep -E '^TEST frame 321: cancelbuild Pkg36Farm id=[0-9]+ ' "$log"
    fi
    if covers 745; then
        site 745 Pkg36Farm '.* under=0 .* loop=-$'
    fi
    if covers 812; then
        expect "TEST pkg36 lua frame=812 object=ArnorPeasantAdult list=RohanPeasant2Functions"
        expect "TEST pkg36 lua frame=812 object=Pkg36PeasantControl list="
        expect "TEST pkg36 attackinfo frame=812 object=ArnorPeasantAdult resolved=1"
        expect "TEST pkg36 attackinfo frame=812 object=Pkg36PeasantControl resolved=0"
    fi
    if covers 890; then
        site 853 Pkg36RepairClock '.* loop=-$'
        site 857 Pkg36RepairClock '.* loop=WallConstructionLoop$'
        site 857 Pkg36RepairDisabled '.* loop=-$'
        site 857 Pkg36RepairNoTimer '.* loop=-$'
        site 890 Pkg36RepairClock '.* loop=-$'
        awk -v early="$(health 853 Pkg36RepairClock)" -v active="$(health 857 Pkg36RepairClock)" \
            -v disabled="$(health 857 Pkg36RepairDisabled)" -v untimed="$(health 857 Pkg36RepairNoTimer)" \
            'BEGIN { exit !(early > 0 && active > early && early == disabled && early == untimed) }'
    fi
    if covers 960; then
        site 910 Pkg36SelfBuild '.* under=1 .* loop=BuildingBigConstructionLoop$'
        site 960 Pkg36SelfBuild '.* under=0 .* loop=-$'
    fi
    if covers 980; then
        site 980 Pkg36SelfControl '.* under=1 .* loop=-$'
    fi
    if covers 1324; then
        site 1132 IsengardCampKeep '.* rise=-[0-9]+\.[0-9] '
        site 1320 IsengardCampKeep '.* under=1 .* rise=0\.0 loop=BuildingBigConstructionLoop$'
        site 1324 Pkg36NoRiseKeep '.* rise=0\.0 '
    fi
    if covers 1053; then
        grep -E '^TEST pkg36 dock frame=1048 name=Pkg36DaleContact valid=1 x=' "$log"
        grep -E '^TEST pkg36 dock frame=1053 name=Pkg36DaleControl valid=1 x=' "$log"
        contact=$(sed -n 's/^TEST pkg36 dock frame=1048 name=Pkg36DaleContact valid=1 //p' "$log")
        control=$(sed -n 's/^TEST pkg36 dock frame=1053 name=Pkg36DaleControl valid=1 //p' "$log")
        [ "$contact" != "$control" ]
    fi
    if covers 1075; then
        grep -E '^TEST pkg36 horde frame=1075 name=ArnorBreelandPeasantHorde repairing=[1-9][0-9]*$' "$log"
        expect "TEST pkg36 horde frame=1075 name=Pkg36HordeControl repairing=0"
    fi
}
