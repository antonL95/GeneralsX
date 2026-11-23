check() {
    if covers 201; then
        expect 'TEST engaged frame=201 using=0 attacking=0 live-target=0 engaged=0 hit=0 draw-both=0'
    fi
    if covers 208; then
        expect 'TEST engaged frame=208 using=1 attacking=1 live-target=1 engaged=1 hit=0 draw-both=0'
    fi
    if covers 212; then
        expect 'TEST engaged frame=212 using=1 attacking=1 live-target=1 engaged=1 hit=1 draw-both=1'
    fi
    if covers 236; then
        expect 'TEST engaged frame=236 using=0 attacking=1 live-target=1 engaged=1 hit=0 draw-both=0'
    fi
    if covers 244; then
        expect 'TEST engaged frame=244 using=0 attacking=0 live-target=0 engaged=1 hit=0 draw-both=0'
    fi
    if covers 245; then
        expect 'TEST engaged frame=245 using=0 attacking=0 live-target=0 engaged=1 hit=0 draw-both=0'
    fi
    if covers 264; then
        expect 'TEST engaged frame=264 using=0 attacking=0 live-target=0 engaged=1 hit=0 draw-both=0'
    fi
    if covers 272; then
        expect 'TEST engaged frame=272 using=0 attacking=0 live-target=0 engaged=0 hit=0 draw-both=0'
    fi
}
