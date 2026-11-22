#!/bin/sh
set -eu

build=${1:?build directory required}
qa=$(cd "$(dirname "$0")" && pwd)
export PKGS1_TRAINING_EXTRA_INI="$qa/saveload/package20.ini"
export PKGS1_TRAINING_EXTRA_COMMANDS="$qa/saveload/package20.txt"
export PKGS1_TRAINING_EXTRA_GDB="$qa/saveload/package20-fx.gdb"
result=$(mktemp)
trap 'rm -f "$result"' EXIT
checkpoint_states() {
    awk -v checkpoint="$checkpoint" '
        /^TEST pkg20 (held|state|ownership|targeting) frame=(241|243|245|246|269|270|315|440) / {
            split($4, field, "=")
            if (field[2] >= checkpoint)
                print
        }
    ' "$1"
}

for checkpoint in 241 244; do
    if ! sh "$qa/test-save-load-crc.sh" "$build" training "$checkpoint" "$((544 - checkpoint))" > "$result" 2>&1; then
        cat "$result"
        exit 1
    fi
    cat "$result"
    session=$(sed -n '1p' "$result")
    log="$session/continue.log"
    checkpoint_states "$log" > "$session/package20-continued.txt"
    checkpoint_states "$session/resume.log" > "$session/package20-resumed.txt"
    diff -u "$session/package20-continued.txt" "$session/package20-resumed.txt"
done

expect() {
    if ! grep -E "$1" "$log"; then
        printf 'Missing package 20 result: %s\n' "$1" >&2
        return 1
    fi
}

expect '^TEST pkg11 state frame=192 name=AmrothianGuardsmenHorde .*memberhp=1785.0$'
expect '^TEST pkg11 state frame=192 name=Pkg20Stacking .*memberhp=1905.0$'
expect '^TEST pkg11 state frame=242 name=AmrothianGuardsmenHorde .*memberhp=1775.0$'
expect '^TEST pkg20 state frame=195 name=Pkg20Captain hp=800.0/800.0 '
expect '^TEST pkg20 state frame=195 name=Pkg20CaptainControl hp=600.0/800.0 '
expect '^TEST pkg20 state frame=195 name=Pkg20Hero hp=800.0/800.0 '
expect '^TEST pkg11 debris frame=199 prefix=BuildingDamageBits count=0$'
expect '^TEST pkg11 debris frame=200 prefix=BuildingDamageBits count=7$'
expect '^TEST pkg20 state frame=200 name=DolAmrothBattleTower hp=900.0/1500.0 state=1 damaged=1 '
expect '^TEST pkg20 state frame=200 name=Pkg20TowerControl hp=900.0/1500.0 state=1 damaged=1 '
expect '^TEST pkg20 state frame=205 name=CaHRestlessDeadBarrow hp=950.0/1250.0 state=1 damaged=1 really=0 '
expect '^TEST pkg20 state frame=205 name=Pkg20BarrowControl hp=950.0/1250.0 state=0 damaged=0 really=0 '
expect '^TEST pkg20 state frame=207 name=CaHRestlessDeadBarrow hp=450.0/1250.0 state=2 damaged=0 really=1 '
expect '^TEST pkg20 state frame=207 name=Pkg20BarrowControl hp=450.0/1250.0 state=1 damaged=1 really=0 '
expect '^TEST pkg20 state frame=209 name=CaHRestlessDeadBarrow hp=1250.0/1250.0 state=0 damaged=0 really=0 '
expect '^TEST pkg20 state frame=209 name=Pkg20BarrowControl hp=1250.0/1250.0 state=0 damaged=0 really=0 '
expect '^TEST pkg20 state frame=281 name=CaHRestlessDeadBarrow hp=0.0/1250.0 state=3 .*death=NORMAL$'
expect '^TEST pkg20 state frame=281 name=Pkg20BarrowControl hp=0.0/1250.0 state=3 .*death=BURNED$'
expect '^TEST emotion frame=221 unit=Pkg20Cheerer type=CHEER source=DurinTheLastStatue '
expect '^TEST pkg20 state frame=222 name=Pkg20Cheerer .*cheer=1 '
expect '^TEST pkg20 state frame=222 name=Pkg20Cheerer .*cheer=0 '
expect '^TEST pkg20 state frame=214 name=RockRohan01 hp=1.0/1.0 '
expect '^TEST pkg11 state frame=214 name=RockRohan01 .*vendor=GenericThrowableRock grab=0,16,0,0 '
expect '^TEST pkg11 state frame=214 name=Pkg20RockVendor .*vendor=RockGreyThrown01 grab=0,8,10,1 '
expect '^TEST pkg11 state frame=243 name=IthilienRuinedTower .*vendor=EntThrownBuildingRock grab=0,16,0,1 '
expect '^TEST pkg11 state frame=243 name=Pkg20RuinsControl .*vendor=GenericThrowableRock grab=490,8,10,0 '
expect '^TEST pkg20 held frame=190 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=GenericThrowableRock '
expect '^TEST pkg20 held frame=190 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=RockGreyThrown01 '
expect '^TEST pkg20 held frame=243 caster=Pkg20ImmediateEnt id=[1-9][0-9]* name=EntThrownBuildingRock '
rock=$(sed -n 's/^TEST frame 180: create GenericThrowableRock for PlyrCreeps id=\([0-9][0-9]*\).*/\1/p' "$log")
expect "^TEST pkg20 held frame=243 caster=RohanEntOak id=$rock name=GenericThrowableRock "
expect '^TEST pkg20 state frame=245 name=Pkg20Radius .*last-member-hit=240 '
expect '^TEST pkg20 state frame=246 name=Pkg20Infantry hp=900.0/1000.0 '
expect '^TEST pkg20 state frame=246 name=Pkg20InfantryControl hp=907.0/1000.0 '
expect '^TEST pkg20 state frame=269 name=Pkg20Infantry hp=900.0/1000.0 '
expect '^TEST pkg20 state frame=270 name=Pkg20Infantry hp=907.0/1000.0 '
expect '^TEST pkg20 targeting frame=315 caster=MordorMountainTroll valid=0 allowed=0 accepted=0 pending=0$'
expect '^TEST pkg20 held frame=440 caster=MordorMountainTroll id=0 name=- '
expect '^TEST pkg20 ownership frame=243 caster=Pkg20ImmediateEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1$'
expect '^TEST pkg20 ownership frame=440 caster=Pkg20FindRockEnt vendor-hostile=1 stock=GenericThrowableRock neutral=1 producer=1$'

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
