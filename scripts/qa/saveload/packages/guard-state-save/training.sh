check() {
    if covers 244; then
        expect 'TEST guard-save frame=244 guard=1 attacking=1'
    fi
    if covers 245; then
        expect 'TEST guard-save frame=245 guard=1 attacking=1'
    fi
}
