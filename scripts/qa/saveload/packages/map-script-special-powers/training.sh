check() (
    if covers 242; then
        expect 'TEST map-script-power auto 230 0 power=1 behavior=1 button=1 auto=1'
        expect 'TEST map-script-power marker 234 team 1'
        expect 'TEST map-script-power marker 238 nearest 1'
        expect 'TEST map-script-power auto 242 1 power=1 behavior=1 button=1 auto=1'
    fi
    if covers 260; then
        expect 'TEST map-script-power auto 248 1 power=1 behavior=1 button=1 auto=1'
        expect 'TEST map-script-power marker 252 owned 1'
        expect 'TEST map-script-power auto 256 0 power=1 behavior=1 button=1 auto=1'
        expect 'TEST map-script-power marker 260 waypoint 1'
    fi
)
