sample() {
    if covers "$2"; then
        expect "PKG68 $1 frame=$2 $3"
    fi
}

loop_event() {
    if covers "$2"; then
        expect "RETAIL AUDIO $1 frame=$2 owner=$3 sound=CallTheHordeOrcPitLoop"
    fi
}

check() {
    sample state 224 'producer=MordorOrcPit index=0 model=0 unit=MordorFighterHorde progress=0.400 speed=1.000'
    sample state 227 'producer=MordorOrcPit index=1 model=1 unit=- progress=-1.000 speed=20.000'
    sample state 244 'producer=MordorOrcPit index=0 model=1 unit=MordorFighterHorde progress=63.889 speed=20.000'
    sample state 244 'producer=MordorOrcPit_WotR index=0 model=1 unit=MordorFighterHorde_WOTRBattle progress=3.067 speed=1.000'
    sample state 270 'producer=MordorOrcPit index=0 model=1 unit=- progress=-1.000 speed=20.000'
    sample state 1665 'producer=MordorOrcPit index=1 model=1 unit=MordorFighterHorde progress=13.889 speed=20.000'
    sample state 1667 'producer=MordorOrcPit index=1 model=0 unit=MordorFighterHorde progress=0.933 speed=1.000'

    sample spawn 257 'producer=MordorOrcPit index=0 unit=MordorFighterHorde found=0'
    sample spawn 258 'producer=MordorOrcPit index=0 unit=MordorFighterHorde found=1'
    sample spawn 971 'producer=MordorOrcPit_WotR index=0 unit=MordorFighterHorde_WOTRBattle found=0'
    sample spawn 972 'producer=MordorOrcPit_WotR index=0 unit=MordorFighterHorde_WOTRBattle found=1'
    sample spawn 2427 'producer=MordorOrcPit index=2 unit=MordorFighterHorde found=0'
    sample spawn 2427 'producer=MordorOrcPit_WotR index=1 unit=MordorFighterHorde_WOTRBattle found=0'
    sample spawn 2428 'producer=MordorOrcPit index=2 unit=MordorFighterHorde found=1'
    sample spawn 2428 'producer=MordorOrcPit_WotR index=1 unit=MordorFighterHorde_WOTRBattle found=1'

    sample spawn 240 'producer=Pkg68ProtectedPit index=0 unit=Pkg68ProtectedRecruit found=0'
    sample spawn 241 'producer=Pkg68ProtectedPit index=0 unit=Pkg68ProtectedRecruit found=1'
    sample hit 419 'unit=Pkg68ProtectedRecruit kind=unresistable protected=1 dealt=0.0'
    sample hit 420 'unit=Pkg68ProtectedRecruit kind=slash protected=1 dealt=0.0'
    if covers 421; then
        grep -Eq '^PKG68 hit frame=421 unit=Pkg68ProtectedRecruit kind=slash protected=0 dealt=[1-9][0-9]*\.[0-9]$' "$log"
    fi

    sample protection 244 'unit=Pkg68DisabledRecruit disabled=1 protected=1'
    sample protection 420 'unit=Pkg68DisabledRecruit disabled=1 protected=1'
    sample hit 420 'unit=Pkg68DisabledRecruit kind=slash protected=1 dealt=0.0'
    sample protection 421 'unit=Pkg68DisabledRecruit disabled=1 protected=0'
    sample hit 421 'unit=Pkg68DisabledRecruit kind=slash protected=0 dealt=7.0'

    stock=$(sed -n 's/^TEST frame 211: create MordorOrcPit for Player_1 id=\([0-9]*\) at (980\.0, 1280\.0)$/\1/p' "$out/continue.log")
    sound_only=$(sed -n 's/^TEST frame 200: create Pkg68ProtectedPit for Player_1 id=\([0-9]*\) .*/\1/p' "$out/continue.log")
    if covers 227; then
        sample state 227 'producer=Pkg68ProtectedPit index=0 model=0 unit=Pkg68ProtectedRecruit progress=56.667 speed=20.000'
        loop_event start 227 "$stock"
        loop_event start 227 "$sound_only"
    fi
    if [ "$phase" = resume ] && [ "$save_frame" -eq 244 ]; then
        loop_event start 244 "$stock"
        loop_event start 244 "$sound_only"
    fi
    loop_event stop 435 "$sound_only"
    loop_event stop 1666 "$stock"
}
