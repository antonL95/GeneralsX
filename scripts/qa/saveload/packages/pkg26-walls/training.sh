check() {
    if covers 220; then
        expect 'TEST pkg26 castle-member frame=220 object=MenFortressCitadel owner=Player_1 hasModule=1 linked=1 isKeep=1 owned=1 storedPrice=0'
        expect 'TEST pkg26 castle-member frame=220 object=MenFortressExpansionPadSide owner=Player_1 hasModule=1 linked=1 isKeep=0 owned=1 storedPrice=0'
        expect 'TEST pkg26 castle-member frame=220 object=Pkg26StandaloneTower owner=Player_1 hasModule=1 linked=0 isKeep=0 owned=0 storedPrice=0'
        expect 'TEST pkg26 portal frame=220 object=MenWallPosternGate target=Pkg26Fighter active=1 canPassTarget=1 canPassPlan=1'
        expect 'TEST pkg26 portal frame=220 object=MenWallPosternGate target=Pkg26Trebuchet active=1 canPassTarget=0 canPassPlan=0'
        expect 'TEST pkg26 portal frame=220 object=MenWallPosternGate target=Pkg26Fighter active=1 canPassTarget=0 canPassPlan=0'
        expect 'TEST pkg26 gate frame=220 object=Pkg26DoorProxy resolved=Pkg26ProxyDoor wantsOpen=0 canPassTarget=0 canPassPlan=0'
        expect 'TEST pkg26 gate frame=220 object=Pkg26DoorProxy resolved=Pkg26DoorProxy wantsOpen=0 canPassTarget=0 canPassPlan=0'
        expect 'TEST pkg26 wall-upgrade frame=220 object=TDHEreborSecretGate hasModule=1 hasWall=1 waiting=0 storedPrice=0'
        expect 'TEST pkg26 wall-upgrade frame=220 object=Pkg26PlainWall hasModule=0 hasWall=0 waiting=0 storedPrice=0'
    fi
    if covers 230; then
        expect 'TEST pkg26 rebuild frame=230 object=Pkg26RubbleWall dead=1 underConst=0 rebuildFrames=60 waiting=0 rebuilding=0'
        expect 'TEST pkg26 rebuild frame=230 object=Pkg26RubbleControl dead=1 underConst=0 rebuildFrames=0 waiting=0 rebuilding=0'
        expect 'TEST pkg26 wall-upgrade frame=230 object=TDHEreborSecretGate hasModule=1 hasWall=1 waiting=1 storedPrice=0'
    fi
    if covers 252; then
        expect 'TEST pkg26 castle-member frame=252 object=MenFortressCitadel owner=PlyrCreeps hasModule=1 linked=1 isKeep=1 owned=1 storedPrice=0'
        expect 'TEST pkg26 castle-member frame=252 object=MenFortressExpansionPadSide owner=PlyrCreeps hasModule=1 linked=1 isKeep=0 owned=1 storedPrice=0'
        expect 'TEST pkg26 castle-member frame=252 object=Pkg26StandaloneTower owner=Player_1 hasModule=1 linked=0 isKeep=0 owned=0 storedPrice=0'
    fi
    if covers 255; then
        expect 'TEST pkg26 gate frame=255 object=Pkg26DoorProxy resolved=Pkg26ProxyDoor wantsOpen=1 canPassTarget=1 canPassPlan=1'
        expect 'TEST pkg26 gate frame=255 object=Pkg26DoorProxy resolved=Pkg26ProxyDoor wantsOpen=1 canPassTarget=0 canPassPlan=0'
    fi
    if covers 280; then
        expect 'TEST pkg26 gate frame=280 object=Pkg26DoorProxy resolved=Pkg26ProxyDoor wantsOpen=0 canPassTarget=0 canPassPlan=0'
    fi
    if covers 295; then
        expect 'TEST pkg26 rebuild frame=295 object=Pkg26RubbleWall dead=0 underConst=1 rebuildFrames=60 waiting=0 rebuilding=1'
        expect 'TEST pkg26 rebuild frame=295 object=Pkg26RubbleControl dead=1 underConst=0 rebuildFrames=0 waiting=0 rebuilding=0'
    fi
    if covers 325; then
        expect 'TEST frame 325: no MenFortressExpansionPadSide 0 of PlyrCreeps'
        expect 'TEST pkg26 castle-member frame=325 object=Pkg26StandaloneTower owner=Player_1 hasModule=1 linked=0 isKeep=0 owned=0 storedPrice=0'
    fi
    if covers 365; then
        expect 'TEST pkg26 rebuild frame=365 object=Pkg26RubbleWall dead=0 underConst=0 rebuildFrames=60 waiting=0 rebuilding=0'
        expect 'TEST pkg26 rebuild frame=365 object=Pkg26RubbleControl dead=1 underConst=0 rebuildFrames=0 waiting=0 rebuilding=0'
        expect 'TEST pkg26 wall-upgrade frame=365 object=TDHEreborSecretGate hasModule=1 hasWall=1 waiting=0 storedPrice=0'
    fi
    if covers 430; then
        awk '$1 == "TEST" && $2 == "track" && $3 == 365 && $5 == "Pkg26Fighter" { unit=$4 }
            $1 == "TEST" && $2 == "pos" && $3 >= 366 && $3 <= 430 && $4 == unit &&
            $6 >= 1090 && $6 <= 1110 && $7 >= 1390 && $7 <= 1410 { crossed=1 }
            END { if (!crossed) print "Friendly fighter did not use the postern"; exit !crossed }' "$log"
    fi
    if covers 455; then
        awk '$1 == "TEST" && $2 == "track" && $3 == 431 && $5 == "Pkg26Fighter" { enemy=$4 }
            $1 == "TEST" && $2 == "pos" && $3 >= 431 && $3 <= 455 && $4 == enemy {
                if ($10 == 1) moved=1
                if ($6 >= 1090 && $6 <= 1110 && $7 >= 1302 && $7 <= 1498) crossed=1
            }
            END { if (!moved || crossed) print "Enemy fighter did not route around the postern"; exit (!moved || crossed) }' "$log"
    fi
    if covers 520; then
        awk '/TEST frame 520: object .* Pkg26UpgradeSegment pos .* dead 0 construction / { found=1 } END { exit !found }' "$log"
    fi
    if covers 1170; then
        expect 'TEST pkg26 wall-upgrade frame=1170 object=MenWallPosternGateSmall hasModule=0 hasWall=0 waiting=0 storedPrice=0'
    fi
}
