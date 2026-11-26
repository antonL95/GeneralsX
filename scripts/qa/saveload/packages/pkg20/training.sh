expect_re() {
    if ! grep -Eq "$1" "$log"; then
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    if covers 190; then
        expect_re '^TEST pkg20 held frame=190 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=GenericThrowableRock '
        expect_re '^TEST pkg20 held frame=190 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=RockGreyThrown01 '
    fi
    if covers 192; then
        expect_re '^TEST pkg11 state frame=192 name=Pkg20Horde .*memberhp=1785.0$'
        expect_re '^TEST pkg11 state frame=192 name=Pkg20Stacking .*memberhp=1905.0$'
    fi
    if covers 195; then
        expect_re '^TEST pkg20 state frame=195 name=Pkg20Captain hp=800.0/800.0 '
        expect_re '^TEST pkg20 state frame=195 name=Pkg20CaptainControl hp=600.0/800.0 '
        expect_re '^TEST pkg20 state frame=195 name=Pkg20Hero hp=800.0/800.0 '
        awk '
            function near(x, y, targetX, targetY) {
                return (x - targetX)^2 + (y - targetY)^2 < 0.09
            }
            /^TEST frame 180: create Pkg20(Captain|Hero|CaptainControl) / {
                split($8, field, "=")
                objectID[$5] = field[2]
            }
            /^TEST frame 190: object [0-9]+ Pkg20ImmediateEnt pos / {
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
    if covers 205; then
        expect_re '^TEST pkg20 state frame=205 name=Pkg20Barrow hp=950.0/1250.0 state=1 damaged=1 really=0 '
        expect_re '^TEST pkg20 state frame=205 name=Pkg20BarrowControl hp=950.0/1250.0 state=0 damaged=0 really=0 '
    fi
    if covers 207; then
        expect_re '^TEST pkg20 state frame=207 name=Pkg20Barrow hp=450.0/1250.0 state=2 damaged=0 really=1 '
        expect_re '^TEST pkg20 state frame=207 name=Pkg20BarrowControl hp=450.0/1250.0 state=1 damaged=1 really=0 '
    fi
    if covers 209; then
        expect_re '^TEST pkg20 state frame=209 name=Pkg20Barrow hp=1250.0/1250.0 state=0 damaged=0 really=0 '
        expect_re '^TEST pkg20 state frame=209 name=Pkg20BarrowControl hp=1250.0/1250.0 state=0 damaged=0 really=0 '
    fi
    if covers 214; then
        expect_re '^TEST pkg20 state frame=214 name=Pkg20Rock hp=1.0/1.0 '
        expect_re '^TEST pkg11 state frame=214 name=Pkg20Rock .*vendor=GenericThrowableRock grab=0,16,0,0 '
        expect_re '^TEST pkg11 state frame=214 name=Pkg20RockVendor .*vendor=RockGreyThrown01 grab=0,8,10,1 '
    fi
    if covers 221; then
        expect_re '^TEST emotion frame=221 unit=Pkg20Cheerer type=CHEER source=Pkg20Statue '
    fi
    if covers 222; then
        expect_re '^TEST pkg20 state frame=222 name=Pkg20Cheerer .*cheer=1 '
        expect_re '^TEST pkg20 state frame=222 name=Pkg20Cheerer .*cheer=0 '
    fi
    if covers 242; then
        expect_re '^TEST pkg11 state frame=242 name=Pkg20Horde .*memberhp=1775.0$'
    fi
    if covers 243; then
        expect_re '^TEST pkg11 state frame=243 name=Pkg20Ruins .*vendor=EntThrownBuildingRock grab=0,16,0,1 '
        expect_re '^TEST pkg11 state frame=243 name=Pkg20RuinsControl .*vendor=GenericThrowableRock grab=490,8,10,0 '
        expect_re '^TEST pkg20 held frame=243 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=EntThrownBuildingRock '
        rock=$(sed -n 's/^TEST frame 180: create Pkg20ThrowableRock for PlyrCreeps id=\([0-9][0-9]*\).*/\1/p' "$log")
        expect_re "^TEST pkg20 held frame=243 caster=Pkg20EntOak id=$rock name=Pkg20ThrowableRock "
        expect_re '^TEST pkg20 ownership frame=243 caster=Pkg20ImmediateEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1$'
    fi
    if covers 245; then
        expect_re '^TEST pkg20 state frame=245 name=Pkg20Radius .*last-member-hit=240 '
    fi
    if covers 246; then
        expect_re '^TEST pkg20 state frame=246 name=Pkg20Infantry hp=900.0/1000.0 '
        expect_re '^TEST pkg20 state frame=246 name=Pkg20InfantryControl hp=907.0/1000.0 '
    fi
    if covers 269; then
        expect_re '^TEST pkg20 state frame=269 name=Pkg20Infantry hp=900.0/1000.0 '
    fi
    if covers 270; then
        expect_re '^TEST pkg20 state frame=270 name=Pkg20Infantry hp=907.0/1000.0 '
    fi
    if covers 281; then
        expect_re '^TEST pkg20 state frame=281 name=Pkg20Barrow hp=0.0/1250.0 state=3 .*death=NORMAL$'
        expect_re '^TEST pkg20 state frame=281 name=Pkg20BarrowControl hp=0.0/1250.0 state=3 .*death=BURNED$'
    fi
    if covers 315; then
        expect 'TEST pkg20 targeting frame=315 caster=Pkg20Troll valid=0 allowed=0 accepted=0 pending=0'
    fi
    if covers 440; then
        expect_re '^TEST pkg20 held frame=440 caster=Pkg20Troll id=0 name=- '
        expect 'TEST pkg20 ownership frame=440 caster=Pkg20FindRockEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1'
    fi
}

compare() {
    awk -v first="$save_frame" '
        /^TEST pkg20 (held|state|ownership|targeting) frame=(241|243|245|246|269|270|281|315|440) / {
            split($4, field, "=")
            if (field[2] >= first)
                print
        }
    ' "$out/continue.log" > "$out/pkg20-continue.txt"
    awk -v first="$save_frame" '
        /^TEST pkg20 (held|state|ownership|targeting) frame=(241|243|245|246|269|270|281|315|440) / {
            split($4, field, "=")
            if (field[2] >= first)
                print
        }
    ' "$out/resume.log" > "$out/pkg20-resume.txt"
    diff -u "$out/pkg20-continue.txt" "$out/pkg20-resume.txt"
}
