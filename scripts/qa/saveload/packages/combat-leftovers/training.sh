check_lines() {
    for line do
        frame=${line#*frame=}
        frame=${frame%% *}
        if covers "$frame"; then
            expect "$line" || return 1
        fi
    done
}

check() {
    check_lines \
        'TEST combat-leftovers gothmog frame=240 label=mounted-before-save mounted=1 avail=restricted ready=1 summoned=0' \
        'TEST combat-leftovers gothmog frame=252 label=mounted-after-save mounted=1 avail=restricted ready=1 summoned=0' \
        'TEST combat-leftovers count frame=255 label=before-expiry player=Player_1 template=CombatLeftoverConverted count=1' \
        'TEST combat-leftovers count frame=255 label=before-expiry-creeps player=PlyrCreeps template=CombatLeftoverConverted count=0' \
        'TEST combat-leftovers gothmog frame=258 label=mounted-after-cast mounted=1 avail=restricted ready=1 summoned=0' \
        'TEST combat-leftovers count frame=265 label=at-boundary player=Player_1 template=CombatLeftoverConverted count=1' \
        'TEST combat-leftovers count frame=265 label=at-boundary-creeps player=PlyrCreeps template=CombatLeftoverConverted count=0' \
        'TEST combat-leftovers gothmog frame=340 label=unmounted-before-cast mounted=0 avail=available ready=1 summoned=0' \
        'TEST combat-leftovers gothmog frame=352 label=unmounted-after-cast mounted=0 avail=not_ready ready=0 summoned=1'
}
