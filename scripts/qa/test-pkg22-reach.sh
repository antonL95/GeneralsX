#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
state="$build/pkg22-state"
export PKGS1_STATE_DIR="$state"
export PKGS1_TRAINING_EXTRA_INI="$here/saveload/pkg22.ini"
export PKGS1_TRAINING_EXTRA_COMMANDS="$here/saveload/pkg22.txt"
export PKGS1_CRC_START_FRAME=1

check() {
    grep -E "$1" "$2"
}

for run in 1 2; do
    sh "$here/test-save-load-crc.sh" "$build" training 244 121
    log="$state/continue.log"
    check 'TEST pkg22 range frame=156 source=Pkg22Reach weapon=Pkg22RuffianBow .* dz=29.0 range=42.5 eligible=0$' "$log"
    check 'TEST pkg22 range frame=157 source=Pkg22Reach weapon=Pkg22RuffianBow .* dz=30.0 range=43.5 eligible=0$' "$log"
    check 'TEST pkg22 range frame=158 source=Pkg22Reach weapon=Pkg22RuffianBow .* dz=64.0 range=74.1 eligible=1$' "$log"
    check 'TEST pkg22 range frame=158 source=Pkg22ReachControl weapon=Pkg22RuffianControl .* dz=64.0 range=42.5 eligible=0$' "$log"
    check 'TEST pkg22 fire frame=158 source=Pkg22Reach weapon=Pkg22RuffianBow .* damage=9.0$' "$log"
    check 'TEST pkg22 fire frame=158 source=Pkg22ReachControl .* damage=0.0$' "$log"
    check 'TEST pkg22 fire frame=160 source=Pkg22DodgeShot .* target=Pkg22DodgeTarget .* damage=0.0$' "$log"
    check 'TEST pkg22 fire frame=161 source=Pkg22DodgeShot .* target=Pkg22Static .* damage=10.0$' "$log"
    check 'TEST projectile frame=162 weapon=Pkg22WallScatter .* victim=0 dest=' "$log"
    check 'TEST projectile frame=163 weapon=Pkg22WallScatter .* victim=[1-9][0-9]* dest=2975.0,540.0$' "$log"
    check 'TEST pkg22 fire frame=163 .* layer=15 ' "$log"
    check 'TEST pkg22 swoop frame=189 source=Pkg22ClawBird weapon=EagleClawAttack active=1$' "$log"
    check 'TEST pkg22 swoop frame=189 source=Pkg22LevelBird weapon=Pkg22LevelFlight active=0$' "$log"
    check 'TEST pkg22 footprint frame=196 primary-ready=1 primary-range=1 secondary-ready=1 secondary-range=0$' "$log"
    check 'TEST pkg22 range frame=196 source=Pkg22FootprintShooter weapon=Pkg22RuffianBow target=Pkg22RangeStructure dz=64.0 .* eligible=1$' "$log"
    check 'TEST pkg22 fire frame=196 source=Pkg22FootprintShooter weapon=Pkg22RuffianBow .* damage=9.0$' "$log"
    check 'TEST impact frame=215 weapon=Pkg22LinearControl .* pos=2900.0,720.0 ' "$log"
    check 'TEST cooldown frame=240 weapon=MordorCaveTrollRampage .* phase=between frames=30$' "$log"
    check 'TEST pkg22 fire frame=269 source=Pkg22Rampage .* ready=0 ' "$log"

    for phase in continue resume; do
        phase_log="$state/$phase.log"
        check 'TEST pkg22 flight frame=244 source=DwarvenGimli id=[1-9][0-9]* alive=1 thrown=1$' "$phase_log"
        check 'TEST pkg22 flight frame=300 source=DwarvenGimli id=[1-9][0-9]* alive=1 thrown=0$' "$phase_log"
        check 'TEST impact frame=2[4-9][0-9] weapon=GondorAragornDwarfTossWarhead .* bonus=1.20,1.00$' "$phase_log"
        check 'TEST impact frame=270 weapon=MordorCaveTrollRampage .* pos=2890.0,670.0 ' "$phase_log"
        check 'TEST cooldown frame=270 weapon=MordorCaveTrollRampage .* phase=between frames=24$' "$phase_log"
        check 'TEST impact frame=354 weapon=MordorCaveTrollRampage .* pos=2920.0,650.0 ' "$phase_log"
        check 'TEST impact frame=354 weapon=MordorCaveTrollRampage .* pos=2900.0,650.0 ' "$phase_log"
    done

    check 'TEST pkg22 fire frame=192 .* ready=1 held=1 remaining=1 projectile=0 ' "$log"
    check 'TEST pkg22 fire frame=241 .* ready=1 held=2 remaining=1 projectile=[1-9][0-9]* ' "$log"
    check 'TEST projectile frame=241 weapon=GondorAragornDwarfToss .* projectile=DwarvenGimli ' "$log"
    check 'TEST pkg22 aim frame=171 weapon=Pkg22Bone named=1 ' "$log"
    check 'TEST pkg22 aim frame=171 weapon=Pkg22MissingBone named=0 ' "$log"
    awk '
        /^TEST projectile frame=164 weapon=Pkg22(Shared|Independent) / {
            key = $4
            aim[key, ++shots[key]] = $NF
        }
        /^TEST pkg22 aim frame=171 weapon=Pkg22Bone / {
            named = substr($7, 5)
            bone = substr($8, 6)
        }
        /^TEST pkg22 aim frame=171 weapon=Pkg22MissingBone / { fallback = substr($7, 5) }
        /^TEST pkg22 flight frame=244 / { airborneID = $6 }
        /^TEST pkg22 flight frame=300 / { landedID = $6 }
        /^TEST projectile frame=241 weapon=GondorAragornDwarfToss / { payloadID = $7 }
        /^TEST impact frame=240 weapon=MordorCaveTrollRampage / { earlyImpact = 1 }
        END {
            shared = "weapon=Pkg22Shared"
            independent = "weapon=Pkg22Independent"
            exit !(shots[shared] == 2 && aim[shared, 1] == aim[shared, 2] &&
                shots[independent] == 2 && aim[independent, 1] != aim[independent, 2] &&
                named != "" && named == bone && fallback != "" && named != fallback &&
                airborneID != "" && airborneID == landedID && landedID == payloadID && !earlyImpact)
        }
    ' "$log"

    awk '
        /^TEST xp-crc / {
            split($3, frame, "=")
            if (frame[2] != ++count || length($4) != 12) exit 1
            print frame[2], $4
        }
        END { if (count != 365) exit 1 }
    ' "$log" > "$state/run-$run.crc"
done

cmp "$state/run-1.crc" "$state/run-2.crc"
printf '%s\n' 'Training Grounds matched across two runs for all 365 frames'
