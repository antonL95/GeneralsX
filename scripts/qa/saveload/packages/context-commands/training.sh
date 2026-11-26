check() {
    if covers 100; then
        expect 'TEST context-click frame=100 unit=MordorFighterHorde target=ContextCaptureTargetA sent=1 special=1'
        expect 'TEST context-click frame=101 unit=GuldurGreatSpider target=ContextCaptureTargetB sent=0 special=0'
        expect 'TEST context-click frame=102 unit=ArbiterofZigurHorde target=ContextCaptureTargetC sent=1 special=1'
        expect 'TEST context-click frame=103 unit=HaradCultistsofZigurHorde target=ContextCaptureTargetD sent=1 special=1'
        expect 'TEST context-click frame=110 unit=ContextTreeGrabber target=tree sent=1 special=1'
    fi
    if covers 400; then
        expect 'TEST context-held frame=400 tree=1 club=1'
    fi
    if covers 420; then
        expect 'TEST context-eat frame=420 available=0 ready=1'
        expect 'TEST context-power frame=430 ready=1'
        expect 'TEST context-held frame=430 tree=1 club=1'
    fi
    if covers 800; then
        for target in A C D; do
            expect "TEST context-owner frame=800 template=ContextCaptureTarget$target captured=1"
        done
        expect 'TEST context-owner frame=800 template=ContextCaptureTargetB captured=0'
    fi
}
