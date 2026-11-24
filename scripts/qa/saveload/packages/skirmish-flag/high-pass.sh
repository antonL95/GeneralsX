check() {
    if ! grep -q '^45 create Player_2 MordorGorbag ' "$out/continue.txt"; then
        return 0
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
