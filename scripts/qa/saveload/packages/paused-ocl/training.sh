check() {
    if covers 189; then
        expect "TEST paused-ocl-probe frame=189 ready=1 allowed=1 target=1 water=0"
    fi
    if covers 209; then
        expect "TEST paused-ocl-probe frame=209 ready=1 allowed=1 target=1 water=0"
    fi
    if covers 234; then
        expect "TEST paused-ocl-probe frame=234 ready=1 allowed=1 target=1 water=0"
    fi
    if covers 195; then
        expect "TEST paused-ocl frame=195 object=PausedOclFortressMarker alive=0"
        expect "TEST paused-ocl frame=195 object=PausedOclCirdanMarker alive=0"
    fi
    if covers 225; then
        expect "TEST paused-ocl frame=225 object=PausedOclMarker alive=0"
    fi
    if covers 235; then
        expect "TEST paused-ocl frame=235 object=PausedOclFortressMarker alive=1"
        expect "TEST paused-ocl frame=235 object=PausedOclCirdanMarker alive=1"
    fi
    if covers 340; then
        expect "TEST paused-ocl frame=340 object=PausedOclGothmogMarker alive=0"
    fi
    if covers 275; then
        expect "TEST paused-ocl frame=275 object=PausedOclMarker alive=1"
    fi
    if covers 390; then
        expect "TEST paused-ocl frame=390 object=PausedOclGothmogMarker alive=1"
    fi
    if covers 446; then
        expect "TEST paused-ocl frame=446 object=PausedOclFortressMarker alive=1"
        expect "TEST paused-ocl frame=446 object=PausedOclCirdanMarker alive=1"
        expect "TEST paused-ocl frame=446 object=PausedOclGothmogMarker alive=1"
    fi
    if covers 191; then
        expect "TEST paused-ocl-countdown frame=191 ready=0"
    fi
    if covers 192; then
        expect "TEST paused-ocl-countdown frame=192 ready=1"
    fi
    if covers 194; then
        expect "TEST paused-ocl-countdown frame=194 ready=0"
    fi
    if covers 245; then
        expect "TEST paused-ocl frame=245 object=PausedOclScriptMarker alive=0"
    fi
    if covers 255; then
        expect "TEST paused-ocl frame=255 object=PausedOclScriptMarker alive=1"
    fi
    if covers 257; then
        expect "TEST paused-ocl-countdown frame=257 ready=0"
    fi
    if covers 259; then
        expect "TEST paused-ocl-countdown frame=259 ready=1"
    fi
    if covers 274; then
        expect "TEST paused-ocl frame=274 object=PausedOclScriptMarker alive=1"
    fi
    if covers 228; then
        expect "TEST paused-ocl frame=228 object=PausedOclHeraldMarker alive=0"
    fi
    if covers 286; then
        expect "TEST paused-ocl frame=286 object=PausedOclHeraldMarker alive=1"
    fi
    if covers 288; then
        expect "TEST paused-ocl frame=288 object=PausedOclDurinMarker alive=1"
    fi
    if covers 452; then
        expect "TEST paused-ocl frame=452 object=PausedOclEnemy alive=0"
    fi
}
