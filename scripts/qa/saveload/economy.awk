function expect(frame, event) {
    if (frame >= first && frame <= last && !observed[frame, event]) {
        print "Missing economy event on frame " frame ": " event
        failed = 1
    }
}

function difference(beforeFrame, before, afterFrame, after, amount) {
    if (beforeFrame >= first && afterFrame <= last &&
        (!(before in cash) || !(after in cash) || cash[after] - cash[before] != amount)) {
        print "Expected " amount " resources from " before " to " after
        failed = 1
    }
}

function hit(frame, target, damage, amount) {
    expect(frame, "source=RowdyTubDwarf target=" target " damage=" damage " gained=" amount " lost=" amount)
}

/^TEST pkg23 / {
    split($4, stamp, "=")
    if ($3 == "cash") {
        split($5, label, "=")
        split($6, amount, "=")
        cash[label[2]] = amount[2]
    } else {
        sub(/^TEST pkg23 [^ ]+ frame=[0-9]+ /, "")
        observed[stamp[2], $0] = 1
    }
}

/^TEST pkg08 site frame=29 name=DolAmrothFarm .* paid=300 / {
    observed[29, "DolAmrothFarm recorded a charge of 300"] = 1
}

END {
    expect(13, "type=DolAmrothFarm mode=placement indicator=1 radius=300.0 efficiency=0.791")
    expect(13, "type=DaleShoremansCottageFoundation mode=placement indicator=0 radius=0.0 efficiency=0.000")
    expect(13, "type=Pkg23FarmWithoutIndicator mode=placement indicator=1 radius=300.0 efficiency=0.791")
    expect(28, "type=DolAmrothFarm mode=selected indicator=1 radius=300.0 efficiency=0.791")
    expect(275, "type=Pkg23FarmWithoutIndicator mode=selected indicator=0 radius=0.0 efficiency=0.000")
    expect(29, "DolAmrothFarm recorded a charge of 300")
    difference(22, "before-build", 29, "before-cancel", -550)
    difference(29, "before-cancel", 32, "after-cancel", 300)
    hit(240, "Pkg23PillageTarget", "0.0", 0)
    hit(242, "Pkg23PillageTarget", "10.0", 0)
    hit(245, "MordorPorter", "1.0", 0)
    hit(247, "Pkg23PillageTarget", "10.0", 1)
    hit(249, "Pkg23PillageTarget", "10.0", 0)
    hit(250, "Pkg23PillageTarget", "10.0", 0)
    hit(255, "Pkg23PillageTarget", "10.0", 0)
    hit(256, "Pkg23PillageTarget", "10.0", 1)
    expect(294, "name=DolAmrothGondorBulwark refund=0")
    expect(303, "name=DolAmrothGondorBulwark refund=51")
    expect(308, "name=DolAmrothGondorBulwark refund=0")
    expect(323, "name=DolAmrothGondorBulwark refund=0")
    expect(338, "name=DolAmrothGondorBulwark refund=0")
    difference(400, "basic-deposit-before", 401, "basic-deposit-after", 5)
    difference(410, "upgraded-deposit-before", 411, "upgraded-deposit-after", 125)
    difference(521, "sale-before", 522, "sale-after", 150)
    exit failed
}
