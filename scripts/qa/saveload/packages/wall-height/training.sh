check() {
    if covers 245; then
        expect 'TEST wall-height frame=245 left=1 right=1 same=0 crosses=0'
    fi
    if covers 254; then
        expect 'TEST wall-height frame=254 left=1 right=1 same=1 crosses=1'
    fi
}
