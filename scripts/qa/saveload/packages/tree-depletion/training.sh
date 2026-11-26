expect_tree() {
    if covers "$1"; then
        expect "TEST tree-depletion frame=$1 tree=$2 boxes=$3 toppled=$4"
    fi
}

check() {
    expect_tree 230 1 2 0
    expect_tree 310 1 2 1
    expect_tree 390 1 1 1
    expect_tree 390 2 2 1
    expect_tree 430 2 1 1
    expect_tree 590 1 0 0
    expect_tree 670 2 0 0
    expect_tree 670 3 2 0
    expect_tree 910 3 2 1
    expect_tree 990 3 0 0
    expect_tree 1430 0 1 1
}
