check() {
    if covers 214; then
        expect 'TEST pkg61 range frame=214 weapon=AicanarocStun height=29.0 range=7.50 eligible=0'
        expect 'TEST pkg61 range frame=215 weapon=AicanarocStun height=30.0 range=8.50 eligible=0'
        expect 'TEST pkg61 range frame=216 weapon=AicanarocStun height=64.0 range=41.14 eligible=1'
        expect 'TEST pkg61 range frame=216 weapon=Pkg61StunControl height=64.0 range=7.50 eligible=0'
    fi
    if covers 300; then
        test "$(grep -c '^TEST pkg61 state frame=252 source=Pkg61Target layer=15 ' "$log")" -eq 2
        test "$(grep -Ec '^TEST damage .*weapon=Pkg61WallScatter .*victim=[0-9]+\(Pkg61Target\).*health=350\.00->340\.00' "$log")" -eq 1
        grep -E '^TEST projectile .*weapon=AngmarStoneThrowerRock .* victim=0 dest=' "$log"
        grep -E '^TEST cooldown .*weapon=Pkg61ScatterControl .*phase=between ' "$log"
        if grep -q '^TEST damage .*weapon=Pkg61ScatterControl ' "$log"; then
            return 1
        fi
    fi
    if covers 477; then
        grep -E '^TEST cooldown .*weapon=Pkg61DodgeShot .*phase=between ' "$log"
        grep -E '^TEST damage .*weapon=Pkg61DodgeControl .*victim=[0-9]+\(Pkg61Dodger\).*health=350\.00->340\.00' "$log"
        grep -E '^TEST projectile .*weapon=AuxillaryArcherBow .* victim=0 dest=' "$log"
        if grep -q '^TEST damage .*weapon=Pkg61DodgeShot ' "$log"; then
            return 1
        fi
    fi
    if covers 541; then
        grep -E '^TEST projectile .*weapon=EvilShipRangedBombardWeapon .*projectile=EvilShipBombardProjectile ' "$log"
        awk '
            /^TEST projectile .*weapon=Pkg61SharedBombard / {
                shared[$NF] = 1
                sharedCount++
            }
            /^TEST projectile .*weapon=Pkg61IndependentBombard / {
                independent[$NF] = 1
                independentCount++
            }
            END { exit !(sharedCount == 2 && independentCount == 2 && length(shared) == 1 && length(independent) == 2) }
        ' "$log"
    fi
    if covers 700; then
        expect 'TEST pkg61 state frame=630 source=Pkg61TrollCargo layer=1 swoop=0 cargo=1 last=0'
        grep -E '^TEST projectile .*weapon=CaveTrollGoblinThrow .*projectile=MordorGoblinSwordsman ' "$log"
        grep -E '^TEST pkg61 state frame=681 source=Pkg61TrollCargo layer=1 swoop=0 cargo=1 last=[1-9][0-9]+$' "$log"
    fi
    if covers 805; then
        grep -E '^TEST cooldown .*weapon=MumakilRampage .*phase=between frames=42$' "$log"
        grep -E '^TEST cooldown .*weapon=MumakilRampage .*phase=between frames=36$' "$log"
        grep -E '^TEST damage .*weapon=MumakilRampage .*\(Pkg61Target\)' "$log"
        grep -E '^TEST cooldown .*weapon=Pkg61RampageControl .*phase=between frames=0$' "$log"
    fi
    if covers 880; then
        grep -E '^TEST pkg61 state frame=(855|861) source=Pkg61SwoopActor .*swoop=1 ' "$log"
        grep -E '^TEST pkg61 state frame=861 source=Pkg61LevelFlightActor .*swoop=0 ' "$log"
    fi
    if covers 937; then
        grep -E '^TEST weapon-aim .*weapon=Pkg61PreferredFx ' "$log"
        grep -E '^TEST damage .*weapon=Pkg61PreferredFx .*\(Pkg61BoneTarget\).*dealt=10\.00 ' "$log"
        grep -E '^TEST damage .*weapon=Pkg61DefaultFx .*\(Pkg61BoneTarget\).*dealt=10\.00 ' "$log"
    fi
}
