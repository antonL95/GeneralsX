prepare() {
    if [ "$save_frame" -ge 118 ]; then
        printf '%s snapshot-wall-state Player_1 GondorCastleWallTrebuchet 0\n' "$save_frame"
        printf '%s snapshot-wall-state Player_1 GondorCastleWallSegment 0\n' "$save_frame"
        printf '%s snapshot-wall-occupant Player_1 - 0 WallTrebuchetOccupant\n' "$save_frame"
    fi
}

check() {
    if [ "$save_frame" -ge 118 ]; then
        expect "TEST save-load wall-state frame=$save_frame template=GondorCastleWallTrebuchet walkable=1"
        expect "TEST save-load wall-state frame=$save_frame template=GondorCastleWallSegment walkable=1"
        expect "TEST save-load wall-occupant frame=$save_frame name=WallTrebuchetOccupant layer=15 walkable=1"
    fi
    if [ "$first" -le 124 ] && covers 126; then
        expect 'TEST save-load wall-occupant frame=124 name=WallDeleteOccupant layer=15 walkable=1'
        expect 'TEST save-load wall-occupant frame=124 name=WallGateOccupant layer=15 walkable=1'
        expect 'TEST save-load wall-occupant frame=126 name=WallDeleteOccupant layer=1 walkable=0'
        expect 'TEST save-load wall-occupant frame=126 name=WallGateOccupant layer=1 walkable=0'
        grep '^TEST list 126 Player_1 MenWallGate 0 ' "$log"
    fi
}
