#!/usr/bin/env bash
set -euo pipefail

label=${1:?session label}
build=${2:?build directory}
runner=${3:?High Pass session runner}
case "$label" in
    ''|*[!a-z0-9-]*) exit 2 ;;
esac

fixture="$build/$label-modifiers.ini"
commands="$build/$label-modifiers.txt"
cat > "$fixture" <<'INI'
ChildObject G10TestHall DwarvenHallExpansion
  AddModule
    Behavior = TransportContain ModuleTag_TestContain
      Slots = 2
      AllowInsideKindOf = INFANTRY
    End
  End
End

Object G10TestTarget
  KindOf = INFANTRY
  TransportSlotCount = 1
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 5000
    InitialHealth = 5000
  End
End

Object G10TestRadiusHealer
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = AutoHealBehavior ModuleTag_Heal
    StartsActive = Yes
    Radius = 40
    HealingAmount = 10
    HealingDelay = 1000
    KindOf = INFANTRY
    NonStackable = Yes
  End
End

Weapon G10TestSlash
  AttackRange = 100
  DamageNugget
    Damage = 100
    DamageType = SLASH
  End
End

Weapon G10TestInnateSlash
  AttackRange = 100
  UseInnateAttributes = Yes
  DamageNugget
    Damage = 100
    DamageType = SLASH
  End
End

ModifierList G10TestSnapshotBuff
  Category = BUFF
  Modifier = DAMAGE_MULT 150%
  Modifier = HEALTH 50
  Modifier = SHROUD_CLEARING 25%
  ModelCondition = EMOTION_AFRAID
  Upgrade = Upgrade_ObjectLevel1 Delay:500
End

ModifierList G10TestRegularDamage
  Category = BUFF
  Modifier = DAMAGE_MULT 150%
End
INI

cat > "$commands" <<SESSION
10 combatini - - 0 $fixture
15 create Player_1 G10TestTarget 0 1240,520,0
15 create Player_2 G10TestTarget 0 1300,520,180
15 create Player_1 G10TestTarget 0 1500,820,0
15 create Player_1 G10TestTarget 0 1500,1020,0
20 firetestweapon Player_1 G10TestTarget 0 G10TestSlash,Player_2:G10TestTarget:0
21 modapply Player_1 G10TestTarget 0 DamageMultAttribute01
22 firetestweapon Player_1 G10TestTarget 0 G10TestSlash,Player_2:G10TestTarget:0
23 firetestweapon Player_1 G10TestTarget 0 G10TestInnateSlash,Player_2:G10TestTarget:0
24 modapply Player_1 G10TestTarget 0 G10TestRegularDamage
24 firetestweapon Player_1 G10TestTarget 0 G10TestSlash,Player_2:G10TestTarget:0
25 damage Player_1 G10TestTarget 1 100
25 damage Player_1 G10TestTarget 2 100
30 create Player_1 G10TestHall 0 1510,820,0
30 testcontain Player_1 G10TestHall 0 Player_1:G10TestTarget:1
30 create Player_1 G10TestRadiusHealer 0 1485,1020,0
30 create Player_1 G10TestRadiusHealer 0 1500,1040,0
61 health Player_1 G10TestTarget 1
61 health Player_1 G10TestTarget 2
61 healsource Player_1 G10TestTarget 1
61 healsource Player_1 G10TestTarget 2
65 create Player_2 G10TestTarget 0 1320,565,0
65 modsnapshot Player_2 G10TestTarget 0 G10TestSnapshotBuff,Player_2:G10TestTarget:1
82 checkobjupgrade Player_2 G10TestTarget 1 Upgrade_ObjectLevel1
84 checkobjupgrade Player_2 G10TestTarget 1 Upgrade_ObjectLevel1
166 mods Player_2 G10TestTarget 1
166 checkobjupgrade Player_2 G10TestTarget 1 Upgrade_ObjectLevel1
170 quit - - 0
SESSION

timeout 900 sh "$runner" "$label" "$commands" "$build" > "$build/$label-runner.log"
log="$build/hardname-$label.log"
grep -qx 'Runner exit 0' "$build/$label-runner.log"
grep -E '^\[Inferior 1 .* exited normally\]|^TEST (containment|health 61|heal-source|modifier-snapshot|check-upgrade|mods 166|damage .*weapon=G10Test)' "$log"
awk '
    /^TEST containment frame=30 admitted=1$/ { contained++ }
    /^TEST health 61 Player_1 G10TestTarget 1 / && / health=4910\.00 / { hallHeal++ }
    /^TEST health 61 Player_1 G10TestTarget 2 / && / health=4910\.00 / { radiusHeal++ }
    /^TEST heal-source frame=61 target=1 source=G10TestHall$/ { hallSource++ }
    /^TEST heal-source frame=61 target=2 source=G10TestRadiusHealer$/ { radiusSource++ }
    /^TEST damage frame=20 weapon=G10TestSlash / && / amount=100\.00 / { base++ }
    /^TEST damage frame=22 weapon=G10TestSlash / && / amount=100\.00 / { innateOff++ }
    /^TEST damage frame=23 weapon=G10TestInnateSlash / && / amount=60\.00 / { innateOn++ }
    /^TEST damage frame=24 weapon=G10TestSlash / && / amount=150\.00 / { ordinaryBuff++ }
    /^TEST modifier-snapshot frame=65 exact=1 holds=1 wake=83 maxBonus=50\.0 damage=1\.500$/ { snapshot++ }
    /^TEST check-upgrade frame=82 found=0$/ { beforeUpgrade++ }
    /^TEST check-upgrade frame=84 found=1$/ { afterUpgrade++ }
    /^TEST mods 166 / && / health=[0-9.]+\/5000\.0 / && / damage=1\.000 / && / lists=\[\]/ { expired++ }
    /^TEST check-upgrade frame=166 found=0$/ { revoked++ }
    /^TEST frame 170: quit$/ { quit++ }
    /^\[Inferior 1 .* exited normally\]/ { normal++ }
    END {
        if (contained != 1 || hallHeal != 1 || radiusHeal != 1 || hallSource != 1 || radiusSource != 1 ||
            base != 1 || innateOff != 1 || innateOn != 1 || ordinaryBuff != 1 || snapshot != 1 ||
            beforeUpgrade != 1 || afterUpgrade != 1 || expired != 1 || revoked != 1 || quit != 1 || normal != 1) {
            printf "Checks passed: contained=%d hall=%d radius=%d hallSource=%d radiusSource=%d base=%d innateOff=%d innateOn=%d ordinaryBuff=%d snapshot=%d beforeUpgrade=%d afterUpgrade=%d expired=%d revoked=%d quit=%d normal=%d\n",
                contained, hallHeal, radiusHeal, hallSource, radiusSource, base, innateOff, innateOn, ordinaryBuff,
                snapshot, beforeUpgrade, afterUpgrade, expired, revoked, quit, normal
            exit 1
        }
    }
' "$log"
