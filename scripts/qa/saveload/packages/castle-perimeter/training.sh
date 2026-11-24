check() {
    if grep -Fq 'create Player_1 CastlePerimeterCamp' "$out/commands.txt"; then
        if covers 232; then
            expect 'TEST castle-perimeter area frame=232 points=18 inside=1 cached=1 entered=1'
            expect 'TEST castle-perimeter plot frame=232 index=0 matches=1'
            expect 'TEST castle-perimeter plot frame=232 index=1 matches=1'
        fi
        for frame in 280 320; do
            if covers "$frame"; then
                expect "TEST castle-perimeter area frame=$frame points=18 inside=1 cached=1 entered=0"
                expect "TEST castle-perimeter guard frame=$frame area=1"
            fi
        done
        if covers 348; then
            expect 'TEST castle-perimeter area frame=348 points=0 inside=0 cached=0 entered=0'
        fi
        if covers 350; then
            expect 'TEST castle-perimeter guard frame=350 area=0'
        fi
    fi
    if covers 280; then
        expect 'TEST castle-perimeter wall frame=280 under=1 progress=0 paid=100 captured=1 door=1'
    fi
    for frame in 315 320; do
        if covers "$frame"; then
            expect "TEST castle-perimeter wall frame=$frame under=1 progress=1 paid=100 captured=1 door=0"
        fi
    done
    if covers 342; then
        expect 'TEST castle-perimeter wall frame=342 under=1 progress=1 paid=100 captured=0 door=0'
    fi
    if covers 350; then
        expect 'TEST castle-perimeter wall frame=350 missing'
        awk '/TEST castle-perimeter money frame=345 cash=/ {before = substr($5, 6)}
            /TEST castle-perimeter money frame=350 cash=/ {after = substr($5, 6)}
            END {if (after - before != 100) exit 1; print "Castle wall cancellation refunded 100"}' "$log"
    fi
    if covers 780; then
        expect 'TEST castle-perimeter wall frame=780 under=0 progress=0 paid=100 captured=0 door=0'
    fi
}
