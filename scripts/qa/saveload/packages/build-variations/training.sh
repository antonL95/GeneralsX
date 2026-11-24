expect_probe() {
    covers "$1" || return 0
    grep -Ei "^TEST build-variations $1 $2" "$log" || {
        printf 'Expected frame %s to match %s (%s)\n' "$1" "$2" "$phase" >&2
        grep -F "TEST build-variations $1 " "$log" >&2 || :
        return 1
    }
}

check() {
    scaffold='build-variations-plot count=1 template=GuldurBarricadeExpansion(A model=ubforttwr_bld|B model=ubforttwrB_bld) scale=1\.000 .* under=1 dead=0$'
    egg='build-variations-product count=1 template=TrollDestroyerEgg_They model=none scale=1\.000 .* under=0 dead='
    expect_probe 243 "$scaffold" || return 1
    expect_probe 243 "${egg}0$" || return 1
    expect_probe 245 "$scaffold" || return 1
    expect_probe 245 "${egg}0$" || return 1
    expect_probe 246 "${egg}1$" || return 1
    expect_probe 275 'build-variations-hatch count=0 ' || return 1
    expect_probe 276 'build-variations-hatch count=1 template=MoriaTrollDestroyer_They model=mucavdestr_skn scale=1\.150 .* under=0 dead=0$' || return 1
    expect_probe 1000 'build-variations-plot count=1 template=GuldurBarricadeExpansion(A model=ubforttwr_skn|B model=ubforttwrB_skn) scale=1\.000 .* under=0 dead=0$' || return 1
    if grep -q "^TEST build-variations 1420 " "$log"; then
        expect_probe 1420 'build-variations-product count=1 template=RohanEnt(Fir model=RUEntFir_SKN|Birch model=RUEntBirch_SKN|Oak model=RUEntOak_SKN) scale=1\.000 .* under=0 dead=0$' || return 1
        expect_probe 1421 'singleton template=BuildVariationSingleChoice logic-sample=1$' || return 1
    fi

    if covers 245 && covers 276; then
        awk '
            $1 == "TEST" && $2 == "build-variations" && $3 == 245 && $4 == "build-variations-product" {
                eggX = substr($9, 3)
                eggY = substr($10, 3)
            }
            $1 == "TEST" && $2 == "build-variations" && $3 == 276 && $4 == "build-variations-hatch" {
                dx = substr($9, 3) - eggX - sqrt(2 * 25 * 25)
                dy = substr($10, 3) - eggY
                if (dx * dx + dy * dy <= 0.02 * 0.02)
                    ok = 1
            }
            END { exit !ok }
        ' "$log" || {
            printf 'Expected the hatch at the egg local (25,25) offset after its -45 degree production exit (%s).\n' "$phase" >&2
            return 1
        }
    fi
}

compare() {
    continuous=$(awk -v first="$save_frame" '$1 == "TEST" && $2 == "build-variations" && $3 >= first' "$out/continue.log")
    restored=$(grep -F 'TEST build-variations ' "$out/resume.log" || :)
    if [ "$continuous" != "$restored" ]; then
        printf '%s\n' 'Continuous build results' "$continuous" 'Restored build results' "$restored" >&2
        return 1
    fi
}
