check() {
    for frame in 243 245 253; do
        if covers "$frame"; then
            grep -E "^TEST lua appearance frame=$frame index=[01] known=6 head=[248] shield=[248]$" "$log" || return 1
        fi
    done
    for frame in 243 245; do
        if covers "$frame"; then
            expect "TEST lua geometry frame=$frame authored=1 active=0 parts=1" || return 1
        fi
    done
    for frame in 222 245 248; do
        if covers "$frame"; then
            expect "TEST check-upgrade frame=$frame found=1" || return 1
        fi
    done
    if covers 223; then
        expect 'TEST check-upgrade frame=223 found=0' || return 1
    fi
    if covers 272; then
        expect 'TEST lua fear frame=272 terror=0' || return 1
    fi
    if covers 303; then
        expect 'TEST lua fear frame=303 terror=1' || return 1
    fi
    if covers 325 && covers 345; then
        expect 'TEST lua blast frame=325 health=10000.00' || return 1
        awk '$1 == "TEST" && $2 == "lua" && $3 == "blast" && $4 == "frame=345" {
            split($5, health, "=")
            print
            damaged = health[2] < 10000
        }
        END { exit !damaged }' "$log" || return 1
    fi
}

compare() {
    before=$(sed -n 's/^TEST lua appearance frame=243 //p' "$out/continue.log")
    after=$(sed -n 's/^TEST lua appearance frame=245 //p' "$out/resume.log")
    if [ "$before" != "$after" ]; then
        printf 'Lua creation appearance changed on load. Before: %s. After: %s.\n' "$before" "$after"
        return 1
    fi
    for run in continue resume; do
        grep -E '^TEST lua (appearance frame=253|geometry frame=245|fear frame=303|blast frame=345)' \
            "$out/$run.log" > "$out/lua-$run.txt" || return 1
    done
    diff -u "$out/lua-continue.txt" "$out/lua-resume.txt"
}
