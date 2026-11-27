check() {
    if covers 242; then
        expect "TEST subobject-creation-fade 242 SubObjectCreationFadeTrained produced=1 show=1 start=242 delay=3.0 duration=30.0 progress=0.000"
        expect "TEST subobject-creation-fade 242 SubObjectCreationFadeDirect produced=0 none"
        expect "TEST subobject-creation-fade 242 SubObjectCreationFadeSkip produced=0 none"
    fi
    if covers 243; then
        expect "TEST subobject-creation-fade 243 SubObjectCreationFadeTrained produced=1 show=0 start=243 delay=3.0 duration=30.0 progress=0.000"
    fi
    if covers 245; then
        expect "TEST subobject-creation-fade 245 SubObjectCreationFadeYoung produced=0 none"
        expect "TEST subobject-creation-fade 245 SubObjectCreationFadeTrained produced=1 show=0 start=243 delay=3.0 duration=30.0 progress=0.000"
        expect "TEST subobject-creation-fade 245 SubObjectCreationFadeBoundary produced=0 show=1 start=245 delay=3.0 duration=30.0 progress=0.000"
    fi
    if covers 252; then
        expect "TEST subobject-creation-fade 252 SubObjectCreationFadeTrained produced=1 show=0 start=243 delay=3.0 duration=30.0 progress=0.200"
        expect "TEST subobject-creation-fade 252 SubObjectCreationFadeBoundary produced=0 show=1 start=245 delay=3.0 duration=30.0 progress=0.133"
    fi
    if covers 280; then
        expect "TEST subobject-creation-fade 280 SubObjectCreationFadeTrained produced=1 show=0 start=243 delay=3.0 duration=30.0 progress=1.000"
        expect "TEST subobject-creation-fade 280 SubObjectCreationFadeBoundary produced=0 show=1 start=245 delay=3.0 duration=30.0 progress=1.000"
    fi
}
