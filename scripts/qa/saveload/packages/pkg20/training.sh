expect_re() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if covers 890; then
        expect_re '^TEST pkg20 held frame=890 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=GenericThrowableRock '
        expect_re '^TEST pkg20 held frame=890 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=RockGreyThrown01 '
    fi
    if covers 892; then
        expect_re '^TEST pkg11 state frame=892 name=Pkg20Horde .*memberhp=1785.0$'
        expect_re '^TEST pkg11 state frame=892 name=Pkg20Stacking .*memberhp=1905.0$'
    fi
    if covers 895; then
        expect_re '^TEST pkg20 state frame=895 name=Pkg20Captain hp=800.0/800.0 '
        expect_re '^TEST pkg20 state frame=895 name=Pkg20CaptainControl hp=600.0/800.0 '
        expect_re '^TEST pkg20 state frame=895 name=Pkg20Hero hp=800.0/800.0 '
        awk '
            function near(x, y, targetX, targetY) {
                return (x - targetX)^2 + (y - targetY)^2 < 0.09
            }
            /^TEST frame 880: create Pkg20(Captain|Hero|CaptainControl) / {
                split($8, field, "=")
                objectID[$5] = field[2]
            }
            /^TEST frame 890: object [0-9]+ Pkg20ImmediateEnt pos / {
                if (++casterCount == 1) {
                    rockX = $8 + 8 * cos($12) - 10 * sin($12)
                    rockY = $9 + 8 * sin($12) + 10 * cos($12)
                } else {
                    wallX = $8 + 16 * cos($12)
                    wallY = $9 + 16 * sin($12)
                }
            }
            /^TEST pkg20 fx-list/ {
                split($4, name, "=")
                split($5, address, "=")
                fx[name[2]] = address[2]
            }
            /^TEST pkg20 fx-object/ {
                split($5, address, "=")
                split($6, id, "=")
                if (address[2] == fx["FX_SpellHealUnitHealBuff"] && !healed[id[2]]++)
                    ++hordeRecipients
                if (address[2] == fx["FX_AragornAthelas"])
                    athelas[id[2]]++
            }
            /^TEST pkg20 fx-position/ {
                split($5, address, "=")
                if (address[2] == fx["FX_WallGrab"]) {
                    split($6, x, "=")
                    split($7, y, "=")
                    ++grabEffects
                    if (near(x[2], y[2], wallX, wallY)) ++wallOffset
                    if (near(x[2], y[2], rockX, rockY)) ++rockOffset
                }
            }
            END {
                printf "Healing FX recipients %d, Athelas self %d ally %d excluded %d, grab FX %d wall offset %d rock offset %d\n",
                    hordeRecipients, athelas[objectID["Pkg20Captain"]], athelas[objectID["Pkg20Hero"]],
                    athelas[objectID["Pkg20CaptainControl"]], grabEffects, wallOffset, rockOffset
                exit !(hordeRecipients == 15 && athelas[objectID["Pkg20Captain"]] && athelas[objectID["Pkg20Hero"]] &&
                    !athelas[objectID["Pkg20CaptainControl"]] && casterCount == 2 && grabEffects == 2 && wallOffset == 1 && rockOffset == 1)
            }
        ' "$log"
    fi
    if covers 914; then
        expect_re '^TEST pkg20 state frame=914 name=Pkg20Rock hp=1.0/1.0 '
        expect_re '^TEST pkg11 state frame=914 name=Pkg20Rock .*vendor=GenericThrowableRock grab=0,16,0,0 '
        expect_re '^TEST pkg11 state frame=914 name=Pkg20RockVendor .*vendor=RockGreyThrown01 grab=0,8,10,1 '
    fi
    if covers 942; then
        expect_re '^TEST pkg11 state frame=942 name=Pkg20Horde .*memberhp=1775.0$'
    fi
    if covers 943; then
        expect_re '^TEST pkg11 state frame=943 name=Pkg20Ruins .*vendor=EntThrownBuildingRock grab=0,16,0,1 '
        expect_re '^TEST pkg11 state frame=943 name=Pkg20RuinsControl .*vendor=GenericThrowableRock grab=490,8,10,0 '
        expect_re '^TEST pkg20 held frame=943 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=EntThrownBuildingRock '
        rock=$(sed -n 's/^TEST frame 880: create Pkg20ThrowableRock for PlyrCreeps id=\([0-9][0-9]*\).*/\1/p' "$log")
        [ -n "$rock" ] || rock=$(sed -n 's/^TEST frame 880: create Pkg20ThrowableRock for PlyrCreeps id=\([0-9][0-9]*\).*/\1/p' "$out/continue.log")
        expect_re "^TEST pkg20 held frame=943 caster=Pkg20EntOak id=$rock name=Pkg20ThrowableRock "
        expect_re '^TEST pkg20 ownership frame=943 caster=Pkg20ImmediateEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1$'
    fi
    if covers 945; then
        expect_re '^TEST pkg20 state frame=945 name=Pkg20Radius .*last-member-hit=940 '
    fi
    if covers 946; then
        expect_re '^TEST pkg20 state frame=946 name=Pkg20Infantry hp=900.0/1000.0 '
        expect_re '^TEST pkg20 state frame=946 name=Pkg20InfantryControl hp=907.0/1000.0 '
    fi
    if covers 969; then
        expect_re '^TEST pkg20 state frame=969 name=Pkg20Infantry hp=900.0/1000.0 '
    fi
    if covers 970; then
        expect_re '^TEST pkg20 state frame=970 name=Pkg20Infantry hp=907.0/1000.0 '
    fi
    if covers 981; then
        expect_re '^TEST pkg20 state frame=981 name=Pkg20Barrow hp=0.0/1250.0 state=3 .*death=NORMAL$'
        expect_re '^TEST pkg20 state frame=981 name=Pkg20BarrowControl hp=0.0/1250.0 state=3 .*death=BURNED$'
    fi
    if covers 1015; then
        expect 'TEST pkg20 targeting frame=1015 caster=Pkg20Troll valid=0 allowed=0 accepted=0 pending=0'
    fi
    if covers 1140; then
        expect_re '^TEST pkg20 held frame=1140 caster=Pkg20Troll id=0 name=- '
        expect 'TEST pkg20 ownership frame=1140 caster=Pkg20FindRockEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1'
    fi
}

compare() {
    awk -v first="$save_frame" '
        /^TEST pkg20 (held|state|ownership|targeting) frame=(941|943|945|946|969|970|981|1015|1140) / {
            split($4, field, "=")
            if (field[2] >= first)
                print
        }
    ' "$out/continue.log" > "$out/pkg20-continue.txt"
    awk -v first="$save_frame" '
        /^TEST pkg20 (held|state|ownership|targeting) frame=(941|943|945|946|969|970|981|1015|1140) / {
            split($4, field, "=")
            if (field[2] >= first)
                print
        }
    ' "$out/resume.log" > "$out/pkg20-resume.txt"
    diff -u "$out/pkg20-continue.txt" "$out/pkg20-resume.txt"
}
