check() {
    if covers 260; then
        expect "TEST script-portal-path frame=260 object=PortalPathClimber active=0 direct=0 script=0"
        expect "TEST script-portal-path frame=260 object=PortalPathBlocked active=0 direct=0 script=0"
    fi
    for frame in 360 380; do
        if covers "$frame"; then
            expect "TEST script-portal-path frame=$frame object=PortalPathClimber active=1 direct=0 script=1"
            expect "TEST script-portal-path frame=$frame object=PortalPathBlocked active=1 direct=0 script=0"
        fi
    done
}
