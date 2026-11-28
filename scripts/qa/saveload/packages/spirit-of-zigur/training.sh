require_line() {
    frame=$1
    pattern=$2
    label=$3
    if ! covers "$frame"; then
        return 0
    fi
    line=$(grep -E "$pattern" "$log" | tail -n 1)
    if [ -z "$line" ]; then
        printf 'FAIL [%s] frame=%s missing pattern=%s\n' "$label" "$frame" "$pattern" >&2
        return 1
    fi
    printf 'PASS [%s] %s\n' "$label" "$line"
}

check() {
    require_line 263 '^TEST hero-ability-button frame=263 .* button=Command_PurchaseUpgradeSpiritOfZigur .* enabled=0' cultists-upgrade-locked &&
    require_line 267 '^TEST hero-ability-button frame=267 .* button=Command_PurchaseUpgradeSpiritOfZigur .* enabled=1' cultists-upgrade-unlocked &&
    require_line 270 '^TEST hero-ability-press frame=270 .* button=Command_PurchaseUpgradeSpiritOfZigur .* accepted=1' cultists-upgrade-press &&
    require_line 386 '^TEST hero-ability-probe frame=386 tag=cultists_upgraded .* set=HaradZigurCultistsHordeSpiritOfZigurCommandSet contain=8 members=8' cultists-commandset-upgraded &&
    require_line 386 '^TEST hero-ability-button frame=386 .* button=Command_ZigurCultistsSpiritOfZigur .* enabled=1 .* ready=1' cultists-spirit-btn &&
    require_line 390 '^TEST hero-ability-press frame=390 .* button=Command_ZigurCultistsSpiritOfZigur .* accepted=1' cultists-spirit-press1 &&
    require_line 420 '^TEST hero-ability-world frame=420 tag=cultists_flames template=HaradZigurCultistSpiritOfZigurFlamesCaster .* total=[1-9]' cultists-flames-spawned &&
    require_line 455 '^TEST hero-ability-probe frame=455 tag=cultists_after_cast1 .* contain=3 members=3' cultists-unitcost-deducted &&
    require_line 505 '^TEST hero-ability-probe frame=505 tag=cultists_after_cast2 .* contain=3 members=3 .* active=0' cultists-unitcost-underflow-blocked &&
    require_line 505 '^TEST hero-ability-world frame=505 tag=cultists_flames_after2 template=HaradZigurCultistSpiritOfZigurFlamesCaster .* total=0' cultists-no-second-flames &&
    require_line 543 '^TEST hero-ability-button frame=543 .* button=Command_ZigurArbiterSpiritOfZigur .* enabled=0' arbiter-spirit-locked &&
    require_line 550 '^TEST hero-ability-button frame=550 .* button=Command_ZigurArbiterSpiritOfZigur .* enabled=1 .* ready=1' arbiter-spirit-unlocked &&
    require_line 555 '^TEST hero-ability-press frame=555 .* button=Command_ZigurArbiterSpiritOfZigur .* accepted=1' arbiter-spirit-press &&
    require_line 585 '^TEST hero-ability-world frame=585 tag=arbiter_flames template=HaradZigurArbiterSpiritOfZigurFlamesCaster .* total=[1-9]' arbiter-flames-spawned &&
    require_line 600 '^TEST hero-ability-probe frame=600 tag=arbiter_after_cast .* contain=8 members=8 .* ready=0' arbiter-spirit-recharge
}
