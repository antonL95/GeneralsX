check_state() {
    frame=$1
    object=$2
    pattern=$3
    if covers "$frame" && ! grep -Eq "^TEST pkg37 state frame=$frame object=$object $pattern" "$log"; then
        printf 'Unexpected %s state at frame %s (wanted %s)\n' "$object" "$frame" "$pattern"
        return 1
    fi
}

check_fx() {
    frame=$1
    object=$2
    count=$3
    if covers "$frame"; then
        actual=$(grep -c "^TEST flammable fx frame=$frame source=$object " "$log" || :)
        if [ "$actual" -ne "$count" ]; then
            printf 'Expected %s ignition effects on %s at frame %s, observed %s\n' "$count" "$object" "$frame" "$actual"
            return 1
        fi
    fi
}

check() {
    check_state 232 Pkg37Entwife 'hp=1250.00 aflame=1 burned=0 model=101 burntModel=1 panic=0 ' || return 1
    check_state 255 Pkg37Entwife 'hp=1250.00 aflame=1 burned=0 model=101 burntModel=1 panic=0 ' || return 1
    check_state 335 Pkg37Entwife 'hp=1220.00 aflame=1 burned=0 model=100 burntModel=1 panic=1 ' || return 1
    check_state 365 Pkg37Entwife '.* aflame=1 .* panic=1 .* water=([1-9][0-9]*[.][0-9]|0[.][1-9])$' || return 1
    check_state 465 Pkg37Entwife '.* aflame=0 burned=1 model=010 burntModel=0 panic=0 ' || return 1
    check_state 243 Pkg37EntPanicResume '.* aflame=1 burned=0 model=100 .* panic=1 moving=1 ' || return 1
    check_state 246 Pkg37EntPanicResume '.* aflame=1 burned=0 model=100 .* panic=1 moving=1 ' || return 1
    check_state 380 Pkg37EntPanicResume '.* aflame=0 burned=1 model=010 .* panic=0 ' || return 1
    check_state 255 EvilShip_Interface 'hp=24[0-9][.].. aflame=1 burned=0 ' || return 1
    check_state 266 EvilShip_Interface '.* aflame=0 burned=0 ' || return 1
    check_state 255 Pkg37MarketPlace '.* aflame=1 burned=0 ' || return 1
    check_state 229 HaradMumakilMatriarch '.* aflame=0 .* passengers=5 burnedPassengers=0 ' || return 1
    check_state 232 HaradMumakilMatriarch '.* aflame=1 .* passengers=5 burnedPassengers=0 ' || return 1
    check_state 285 HaradMumakilMatriarch '.* aflame=0 burned=1 .* passengers=5 burnedPassengers=0 ' || return 1
    check_state 354 Pkg37MatriarchDamageableCrew '.* aflame=0 .* passengers=5 burnedPassengers=0 ' || return 1
    check_state 356 Pkg37MatriarchDamageableCrew '.* aflame=1 .* passengers=5 burnedPassengers=5 ' || return 1
    check_state 391 Pkg37MatriarchControl '.* aflame=1 .* passengers=5 burnedPassengers=0 ' || return 1
    check_fx 230 Pkg37Entwife 1 || return 1
    check_fx 230 Pkg37MarketPlace 8 || return 1
    check_fx 230 HaradMumakilMatriarch 1 || return 1
    check_fx 284 HaradMumakilMatriarch 0 || return 1
    check_fx 355 Pkg37MatriarchDamageableCrew 1 || return 1
    check_fx 390 Pkg37MatriarchControl 0 || return 1
    if covers 332 && ! grep -Eq '^TEST flammable water run frame=332 source=Pkg37Entwife ' "$log"; then
        echo 'Entwife did not select a water destination at the end of its animation'
        return 1
    fi
    for object in Pkg37EntShortSearch Pkg37EntDeepSearch Pkg37EntSparseSearch Pkg37EntNoSearch; do
        check_state 650 "$object" '.* aflame=1 burned=0 model=100 .* panic=0 ' || return 1
        if covers 650 && grep -Eq "^TEST flammable water run .* source=$object " "$log"; then
            printf 'Water search control %s selected an unexpected destination\n' "$object"
            return 1
        fi
    done
    check_state 760 Pkg37EntShortSearch '.* aflame=0 burned=1 model=010 burntModel=1 ' || return 1
    check_state 760 Pkg37EntNoSearch '.* aflame=0 burned=0 model=000 burntModel=0 ' || return 1
    check_state 910 Pkg37SpreadSource '.* aflame=1 burned=1 ' || return 1
    check_state 910 Pkg37SpreadTarget '.* aflame=1 ' || return 1
    check_state 910 Pkg37SpreadControl '.* aflame=0 ' || return 1

    for sample in 240:1 250:1 270:1 290:2; do
        frame=${sample%:*}
        if covers "$frame"; then
            grep -Fxq "TEST pkg37 count frame=$frame type=CreateAHeroBreadCrumb_Level1 count=${sample#*:}" "$log" ||
                { echo "Unexpected oil breadcrumb count at frame $frame"; return 1; }
        fi
    done
    if covers 226 && ! grep -Eq '^TEST terrain fire frame=226 burning=0 fuel=0 rate=0 .* x=105 y=765$' "$log"; then
        echo 'Oil control cell had fuel before the spill'
        return 1
    fi
    if covers 245 && ! grep -Eq '^TEST terrain fire frame=245 burning=0 fuel=[1-9][0-9]* rate=0 .* x=105 y=765$' "$log"; then
        echo 'Oil spill did not add fuel to the probed cell'
        return 1
    fi
    for sample in 231:1000.00 250:995.00 275:995.00 345:990.00; do
        check_state "${sample%:*}" Pkg37FieldTarget "hp=${sample#*:} " || return 1
    done
    check_state 250 Pkg37FieldTarget 'hp=1000.00 '
}
