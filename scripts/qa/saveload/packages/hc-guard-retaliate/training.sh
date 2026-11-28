check() {
    if covers 244; then
        expect 'TEST guard-retaliate frame=244 name=GuardRetaliateRanged retaliating=1 attacking=1 melee=0 stdRange=250.00 exitRadius=690.00 chaseFrames=150'
        grep -E '^TEST guard-retaliate frame=244 name=GuardRetaliateMelee retaliating=1 attacking=1 melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 245; then
        expect 'TEST guard-retaliate frame=245 name=GuardRetaliateRanged retaliating=1 attacking=1 melee=0 stdRange=250.00 exitRadius=690.00 chaseFrames=150'
        grep -E '^TEST guard-retaliate frame=245 name=GuardRetaliateMelee retaliating=1 attacking=1 melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 265; then
        grep -E '^TEST guard-retaliate frame=265 name=GuardRetaliateRanged retaliating=[01] attacking=0 melee=0 stdRange=250\.00 exitRadius=690\.00 chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 410; then
        grep -E '^TEST guard-retaliate frame=410 name=GuardRetaliateMelee retaliating=1 attacking=1 melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 475; then
        grep -E '^TEST guard-retaliate frame=475 name=GuardRetaliateMelee retaliating=[01] attacking=0 melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 1235; then
        expect 'TEST guard-retaliate frame=1235 name=GuardRetaliateInnerRanged retaliating=1 attacking=1 melee=0 stdRange=250.00 exitRadius=252.20 chaseFrames=150'
    fi
    if covers 1290; then
        expect 'TEST guard-retaliate frame=1290 name=GuardRetaliateInnerRanged retaliating=0 attacking=0 melee=0 stdRange=250.00 exitRadius=252.20 chaseFrames=150'
    fi
    if covers 775; then
        expect 'TEST guard-retaliate frame=775 name=GuardRetaliateInnerRanged retaliating=1 attacking=1 melee=0 stdRange=250.00 exitRadius=252.20 chaseFrames=150'
    fi
    if covers 830; then
        expect 'TEST guard-retaliate frame=830 name=GuardRetaliateInnerRanged retaliating=1 attacking=1 melee=0 stdRange=250.00 exitRadius=252.20 chaseFrames=150'
    fi
    if covers 955; then
        grep -E '^TEST guard-retaliate frame=955 name=GuardRetaliateInnerMelee retaliating=1 attacking=[01] melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
    if covers 1100; then
        grep -E '^TEST guard-retaliate frame=1100 name=GuardRetaliateInnerMelee retaliating=0 attacking=0 melee=1 stdRange=287\.50 exitRadius=[0-9]+\.[0-9]+ chaseFrames=150$' "$log" >/dev/null
    fi
}
