check() {
    if ! grep -q ' create Player_1 P63StoreBombard ' "$out/commands.txt"; then
        return 0
    fi
    if covers 230; then
        [ "$(grep -c '^TEST marsh route frame=230 connected=1$' "$log")" -eq 2 ] || return 1
    fi
    for frame in 280 290; do
        covers "$frame" || continue
        expect "TEST locomotor-store-water $frame P63StoreBombard river=1 cap=0.5000 height=246" || return 1
        expect "TEST locomotor-store-water $frame P63StoreFullSpeedShip river=1 cap=1.0000 height=246" || return 1
        awk -v frame="$frame" '
            $1 == "TEST" && $2 == "steering" && $3 == frame {
                print
                split(substr($5, 5), position, ",")
                split(substr($18, 8), planar, ",")
                if ($4 == "P63StoreBombard")
                    bombard = position[2] > 1265 && planar[2] > 0.45 && planar[2] < 0.55
                if ($4 == "P63StoreFullSpeedShip")
                    fullSpeed = position[2] > 1265 && planar[2] > 0.9 && planar[2] < 1.1
            }
            END { exit !(bombard && fullSpeed) }
        ' "$log" || return 1
    done
    if covers 250; then
        expect "TEST locomotor-store-water 250 P63StoreBankProbe river=0 cap=1.0000 height=0" || return 1
    fi
    if covers 260; then
        expect "TEST locomotor-store-water 260 P63StoreBankProbe river=1 cap=0.5000 height=366" || return 1
    fi
    if covers 290; then
        awk '
            /^TEST locomotor-store-water 290 P63StoreBankProbe river=1 cap=0.5000 height=/ {
                print
                height = substr($7, 8) + 0
                advancing = height > 376 && height < 396
            }
            END { exit !advancing }
        ' "$log" || return 1
    fi
    if covers 315; then
        expect "TEST locomotor-store-water 315 P63StoreBankProbe river=1 cap=0.5000 height=396" || return 1
    fi
}
