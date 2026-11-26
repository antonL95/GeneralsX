check() (
    if covers 225; then
        expect 'TEST fireweapon-cadence health 226 scaled-slash 88'
        expect 'TEST fireweapon-cadence health 227 scaled-unresistable 73'
        expect 'TEST combat-fields cargo 225 matching=2 other=1'
        expect 'TEST combat-fields slot 235 0 image=UCCatapult_Heads next=2 available=1'
        expect 'TEST combat-fields slot 239 2 image=UCCatapult_Rock next=0 available=1'
        expect 'TEST combat-fields cargo 241 matching=1 other=1'
        expect 'TEST fireweapon-cadence health 241 carrier 100'
    fi
    if covers 263; then
        expect 'TEST combat-fields slot 255 2 image=UCCatapult_Rock next=0 available=1'
        expect 'TEST combat-fields slot 259 0 image=UCCatapult_Heads next=2 available=1'
        expect 'TEST combat-fields slot 262 2 image=UCCatapult_Rock next=0 available=1'
        expect 'TEST combat-fields cargo 263 matching=0 other=1'
    fi
)
