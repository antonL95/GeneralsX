check() {
    if covers 181; then
        expect 'TEST wall-ramp frame=181 wall=1 ramp1=1 ramp2=1 vertices=6,6,6 faces=2,2,2 heights=44.29,68.13,0.00'
        expect 'TEST wall-ramp-shape frame=181 polygon=4 ramp1=1 ramp2=1'
    fi
}
