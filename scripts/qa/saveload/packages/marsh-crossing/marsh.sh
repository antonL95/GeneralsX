prepare() {
    printf '%s passmap - - 0\n' "$save_frame"
    if [ "$save_frame" -ge 25 ]; then
        printf '%s marsh-route Player_1 GondorFaramir 0 - 1405 1075\n' "$save_frame"
    fi
    if [ "$end" -gt "$save_frame" ]; then
        printf '%s passmap - - 0\n' "$((save_frame + 1))"
    fi
}

compare() {
    for phase in continue resume; do
        awk '
            /^TEST passmap / { frame=$3 }
            /^TEST passrow 62 / {
                print frame, substr($4, 204, 1), substr($4, 195, 1), substr($4, 185, 1), substr($4, 176, 1)
            }
        ' "$out/$phase.log" > "$out/$phase.water"
        for probe in 30 41 91 "$save_frame" "$((save_frame + 1))"; do
            if [ "$probe" -gt "$end" ] || { [ "$phase" = resume ] && [ "$probe" -lt "$save_frame" ]; }; then
                continue
            fi
            if [ "$probe" -ge 40 ] && [ "$probe" -lt 90 ]; then
                grep -Fx "$probe 1 1 1 2" "$out/$phase.water"
            else
                grep -Fx "$probe 0 0 1 2" "$out/$phase.water"
            fi
        done
        for probe in 30 41 91 "$save_frame"; do
            if [ "$probe" -lt 25 ] || [ "$probe" -gt "$end" ] ||
                { [ "$phase" = resume ] && [ "$probe" -lt "$save_frame" ]; }; then
                continue
            fi
            if [ "$probe" -ge 40 ] && [ "$probe" -lt 90 ]; then
                log="$out/$phase.log" expect "TEST marsh route frame=$probe connected=0"
            else
                log="$out/$phase.log" expect "TEST marsh route frame=$probe connected=1"
            fi
        done
        awk -v checkpoint="$save_frame" '
            /^TEST passmap / { frame=$3 }
            /^TEST passrow / && frame >= checkpoint { print frame, $3, $4 }
        ' "$out/$phase.log" > "$out/$phase.marsh"
        if [ "$end" -ge 800 ] && { [ "$phase" = continue ] || [ "$save_frame" -le 800 ]; }; then
            grep -E '^TEST health 800 Player_1 GondorFaramir 0 .* dead=0 pos=' "$out/$phase.log" |
                awk '{ split($11, p, "[=,]"); x=p[2]-1405; y=p[3]-1075; arrived=(x*x+y*y<=400); print }
                    END { exit !arrived }'
        fi
    done
    cmp "$out/continue.marsh" "$out/resume.marsh"
}
