check() {
    for frame in 225 243 244 400; do
        if covers "$frame"; then
            flags='power1=1 invulnerable=0 unpacking=1'
            case "$frame" in
                225|400) flags='power1=0 invulnerable=0 unpacking=0' ;;
            esac
            grep -F "TEST which-special-power $frame GondorBoromir $flags health=" "$log" || return 1
            grep -F "TEST which-special-power $frame MordorBalrog $flags health=" "$log" || return 1
        fi
    done

    if covers 244 && covers 278; then
        grep -E '^TEST frame 247: attack object [0-9]+ \(GondorBoromir\) by object [0-9]+ \(WhichSpecialPowerHostile\)$' "$log" || return 1
        grep -F 'TEST which-special-power 278 GondorBoromir power1=1 invulnerable=0 unpacking=1 health=' "$log" || return 1
        awk '
            $1 == "TEST" && $2 == "which-special-power" && $4 == "GondorBoromir" {
                if ($3 == 244 || $3 == 278) {
                    split($8, health, "=")
                    hp[$3] = health[2] + 0
                }
            }
            END {
                if (hp[244] <= 0 || hp[278] <= 0 || hp[278] >= hp[244]) {
                    print "which-special-power: hostile failed to damage Boromir during his cast"
                    exit 1
                }
            }
        ' "$log" || return 1
    fi
}

compare() {
    for phase in continue resume; do
        awk -v first="$save_frame" '$1 == "TEST" && $2 == "which-special-power" && $3 >= first' \
            "$out/$phase.log" > "$out/which-special-power-$phase.txt"
    done
    diff -u "$out/which-special-power-continue.txt" "$out/which-special-power-resume.txt"
}
