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
    require_line 263 '^TEST hero-ability-button frame=263 .* button=Command_MadChieftain in_set=1 .* passive=1' caran-rank1-passive &&
    require_line 263 '^TEST hero-ability-button frame=263 .* button=Command_SpecialAbilityBreakerofStone .* enabled=0' caran-rank1-breaker-locked &&
    require_line 263 '^TEST hero-ability-button frame=263 .* button=Command_SpecialAbilityDerangement .* enabled=0' caran-rank1-derangement-locked &&
    require_line 263 '^TEST hero-ability-button frame=263 .* button=Command_SpecialAbilityCaranLambarPull .* enabled=0' caran-rank1-pull-locked &&
    require_line 270 '^TEST hero-ability-button frame=270 .* button=Command_SpecialAbilityBreakerofStone .* enabled=1 .* ready=1' caran-rank10-breaker-btn &&
    require_line 270 '^TEST hero-ability-button frame=270 .* button=Command_SpecialAbilityDerangement .* enabled=1 .* ready=1' caran-rank10-derangement-btn &&
    require_line 270 '^TEST hero-ability-button frame=270 .* button=Command_SpecialAbilityDireStraits in_set=1 .* passive=1' caran-rank10-direstraits-passive &&
    require_line 270 '^TEST hero-ability-button frame=270 .* button=Command_SpecialAbilityCaranLambarPull .* enabled=1 .* ready=1' caran-rank10-pull-btn &&
    require_line 275 '^TEST hero-ability-press frame=275 .* button=Command_SpecialAbilityBreakerofStone .* accepted=1' caran-breaker-press &&
    require_line 395 '^TEST hero-ability-probe frame=395 tag=breaker_end .* ready=0' caran-breaker-recharge &&
    require_line 395 '^TEST hero-ability-probe frame=395 tag=breaker_tower .* hp=(1[0-4][0-9]{2}|[0-9]{1,3})\.' caran-breaker-damage &&
    require_line 405 '^TEST hero-ability-press frame=405 .* button=Command_SpecialAbilityDerangement .* accepted=1' caran-derangement-press &&
    require_line 490 '^TEST hero-ability-probe frame=490 tag=derangement_end .* ready=0' caran-derangement-recharge &&
    require_line 560 '^TEST hero-ability-press frame=560 .* button=Command_SpecialAbilityCaranLambarPull .* accepted=1' caran-pull-press &&
    require_line 580 '^TEST hero-ability-probe frame=580 tag=pull_early .* mods=.*CaranLambarSlayer' caran-pull-slayer &&
    require_line 670 '^TEST hero-ability-probe frame=670 tag=pull_end .* mods=CaranLambarSlayer' caran-pull-weapon-fired &&
    require_line 698 '^TEST hero-ability-button frame=698 .* button=Command_BurodapanFakeLeadership in_set=1 .* passive=1' buro-rank1-passive &&
    require_line 698 '^TEST hero-ability-button frame=698 .* button=Command_SpecialAbilityDominateEnemyBurodapan .* enabled=0' buro-rank1-dominate-locked &&
    require_line 705 '^TEST hero-ability-button frame=705 .* button=Command_SpecialAbilityDominateEnemyBurodapan .* enabled=1 .* ready=1' buro-rank10-dominate-btn &&
    require_line 705 '^TEST hero-ability-button frame=705 .* button=Command_GiveNothingBack .* enabled=1 .* ready=1' buro-rank10-givenothing-btn &&
    require_line 705 '^TEST hero-ability-button frame=705 .* button=Command_SpecialAbilityWateryGraves .* enabled=1 .* ready=1' buro-rank10-watery-btn &&
    require_line 705 '^TEST hero-ability-button frame=705 .* button=Command_SpecialAbilityHornofKarna .* enabled=1 .* ready=1' buro-rank10-horn-btn &&
    require_line 710 '^TEST hero-ability-press frame=710 .* button=Command_SpecialAbilityDominateEnemyBurodapan .* accepted=1' buro-dominate-press &&
    require_line 770 '^TEST hero-ability-probe frame=770 tag=dominate_caster .* ready=0' buro-dominate-recharge &&
    require_line 770 '^TEST hero-ability-probe frame=770 tag=dominate_converted .* alive=1 .* team=Player_1' buro-dominate-converted &&
    require_line 830 '^TEST hero-ability-press frame=830 .* button=Command_GiveNothingBack .* accepted=1' buro-givenothing-press &&
    require_line 990 '^TEST hero-ability-press frame=990 .* button=Command_SpecialAbilityWateryGraves .* accepted=1' buro-watery-press &&
    require_line 1060 '^TEST hero-ability-probe frame=1060 tag=watery_caster .* ready=0' buro-watery-recharge &&
    require_line 1060 '^TEST hero-ability-probe frame=1060 tag=watery_after .* mods=BurodapanWateryGravesStructureWeapon' buro-watery-mod &&
    require_line 1070 '^TEST hero-ability-press frame=1070 .* button=Command_SpecialAbilityHornofKarna .* accepted=1' buro-horn-press &&
    require_line 1110 '^TEST hero-ability-probe frame=1110 tag=horn_caster .* ready=0' buro-horn-recharge &&
    require_line 1110 '^TEST hero-ability-world frame=1110 tag=horn_spawn template=HaradHornofKarna .* total=[1-9]' buro-horn-spawn
}
