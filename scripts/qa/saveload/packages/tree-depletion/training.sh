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
    for frame in 260 800; do
        if covers "$frame"; then
            expect "TEST tree-depletion selectable frame=$frame TreeLothlorienBase08 object=0 drawable=0"
            expect "TEST tree-depletion selectable frame=$frame MordorLumberMill object=1 drawable=1"
        fi
    done
    if covers 800; then
        expect "TEST tree-depletion selectable frame=800 ElvenWoodTreeOpt object=0 drawable=0"
    fi
    if covers 300; then
        expect "TEST tree-depletion mill frame=300 rank=1 experience=1976"
    fi
    if covers 900; then
        expect "TEST tree-depletion mill frame=900 rank=2 experience=2008"
    fi
    expect_tree 290 5 90 0
    expect_tree 330 5 0 0
    expect_tree 330 6 0 0
    expect_tree 330 7 0 0
    if covers 300; then
        expect "TEST tree-depletion devastate frame=300 module=1 payout=1300"
    fi
}
