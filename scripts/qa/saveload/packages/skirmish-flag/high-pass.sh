prepare() {
    if grep -q '^45 create Player_2 MordorGorbag ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' >> "$out/start.gdb"
    fi
}

check() {
    if ! grep -q '^45 create Player_2 MordorGorbag ' "$out/continue.txt"; then
        return 0
    fi
    if covers 600; then
        grep -Eq '^TEST neutralstate frame=600 object=FlagPairingFixtureFlag .* capture=0 captured=1 .* x=1120\.0 y=1120\.0 ' "$log" &&
        grep -Eq '^TEST neutralstate frame=600 object=FlagPairingFixtureBuilding .* x=1300\.0 y=1120\.0 ' "$log" || {
            printf 'Missing capture at 150 units from the linked building boundary (%s)\n' "$phase"
            return 1
        }
        printf 'PASS capture at 150 units from the linked building boundary (%s)\n' "$phase"
        outside_flag=$(sed -n 's/^TEST neutralstate frame=600 object=FlagPairingFixtureFlag id=\([0-9][0-9]*\) .* captured=0 .* x=1120\.0 y=650\.0 .*/\1/p' "$log")
        [ -n "$outside_flag" ] && grep -Eq "^TEST capture flag=$outside_flag linked=0 " "$log" &&
        grep -Eq '^TEST health 600 PlyrCivilian FlagPairingFixtureBuilding 0 .* dead=0 pos=1301\.0,650\.0$' "$log" || {
            printf 'Pair transferred outside the 150 unit boundary cutoff (%s)\n' "$phase"
            return 1
        }
        printf 'PASS ownership stays neutral at 151 units from the linked building boundary (%s)\n' "$phase"
    fi
    if covers 620; then
        grep -Eq '^TEST neutralstate frame=620 object=CaptureFlag .* capture=1 captured=0 ' "$log" || {
            printf 'Missing AI flag capture preparation (%s)\n' "$phase"
            return 1
        }
    fi
    if covers 1100; then
        grep -Eq '^TEST neutralstate frame=1100 object=CaptureFlag .* owner=4 .* capture=0 captured=1 ' "$log" &&
        grep -Eq '^TEST neutralstate frame=1100 object=SignalFire .* owner=4 .* captured=1 ' "$log" || {
            printf 'Missing AI capture of the flag and its linked signal fire (%s)\n' "$phase"
            return 1
        }
        printf 'PASS targetless capture claims the flag and linked signal fire (%s)\n' "$phase"
    fi
}
