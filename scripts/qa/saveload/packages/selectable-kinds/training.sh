check() {
    for frame in 230 270; do
        if covers "$frame"; then
            expect "TEST selectable-kinds $frame TreeLothlorienBase08 object=1 click=1 drag=1"
            expect "TEST selectable-kinds $frame SelectableKindsDrone object=0 click=0 drag=0"
            expect "TEST selectable-kinds $frame SelectableKindsAlways object=1 click=1 drag=1"
            expect "TEST selectable-kinds $frame SelectableKindsMoveOnly object=1 click=0 drag=0"
        fi
    done
    for frame in 236 276; do
        if covers "$frame"; then
            expect "TEST selectable-kinds $frame TreeLothlorienBase08 selected=1"
        fi
    done
    for frame in 241 281; do
        if covers "$frame"; then
            expect "TEST selectable-kinds $frame SelectableKindsMoveOnly selected=0"
        fi
    done
}
