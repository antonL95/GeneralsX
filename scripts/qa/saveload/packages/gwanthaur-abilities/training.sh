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
    require_line 262 '^TEST hero-ability-button frame=262 .* button=Command_SpecialAbilityGwanthaurSoulTrap .* enabled=1 .* ready=1' gwanthaur-soultrap-btn &&
    require_line 262 '^TEST hero-ability-button frame=262 .* button=Command_SpecialAbilityGwanthaurSacrifice .* enabled=1 .* ready=1' gwanthaur-sacrifice-btn &&
    require_line 262 '^TEST hero-ability-button frame=262 .* button=Command_SpecialAbilityGwanthaurReclamation .* enabled=1 .* ready=1' gwanthaur-reclamation-btn &&
    require_line 262 '^TEST hero-ability-button frame=262 .* button=Command_SpecialAbilityGwanthaurExpulsion .* enabled=1 .* ready=1' gwanthaur-expulsion-btn &&
    require_line 265 '^TEST hero-ability-press frame=265 .* button=Command_SpecialAbilityGwanthaurSoulTrap .* accepted=1' gwanthaur-soultrap-press &&
    require_line 295 '^TEST hero-ability-world frame=295 tag=soultrap_spawn template=GuldurGwanthaurSoulTrap .* total=[1-9]' gwanthaur-soultrap-spawn &&
    require_line 295 '^TEST hero-ability-probe frame=295 tag=soultrap_caster .* ready=0' gwanthaur-soultrap-recharge &&
    require_line 295 '^TEST hero-ability-probe frame=295 tag=soultrap_enemy .* mods=GuldurChillOfGrave' gwanthaur-soultrap-debuff &&
    require_line 320 '^TEST hero-ability-press frame=320 .* button=Command_SpecialAbilityGwanthaurSacrifice .* accepted=1' gwanthaur-sacrifice-press &&
    require_line 350 '^TEST hero-ability-probe frame=350 tag=sacrifice_mid .* contain=1' gwanthaur-sacrifice-grab &&
    require_line 450 '^TEST hero-ability-probe frame=450 tag=sacrifice_end .* ready=0' gwanthaur-sacrifice-recharge &&
    require_line 470 '^TEST hero-ability-press frame=470 .* button=Command_SpecialAbilityGwanthaurReclamation .* accepted=1' gwanthaur-reclamation-press &&
    require_line 530 '^TEST hero-ability-probe frame=530 tag=reclamation_caster .* ready=0' gwanthaur-reclamation-recharge &&
    require_line 530 '^TEST hero-ability-probe frame=530 tag=reclamation_tower .* alive=0' gwanthaur-reclamation-kill &&
    require_line 530 '^TEST hero-ability-world frame=530 tag=reclamation_tree template=GuldurTwistedTree\* .* total=[1-9]' gwanthaur-reclamation-tree &&
    require_line 478 '^TEST hero-ability-world frame=478 tag=reclamation_fx template=GuldurGwanthaurDarkReclamationFX .* total=[1-9]' gwanthaur-reclamation-fx &&
    require_line 560 '^TEST hero-ability-press frame=560 .* button=Command_SpecialAbilityGwanthaurExpulsion .* accepted=1' gwanthaur-expulsion-press &&
    require_line 610 '^TEST hero-ability-probe frame=610 tag=expulsion_caster .* ready=0' gwanthaur-expulsion-recharge &&
    require_line 610 '^TEST hero-ability-world frame=610 tag=expulsion_flood template=GwanthaurShadesFlood .* total=[1-9]' gwanthaur-expulsion-flood &&
    require_line 660 '^TEST hero-ability-probe frame=660 tag=expulsion_enemy .* alive=0' gwanthaur-expulsion-kill
}
