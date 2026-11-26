check() {
    if covers 234; then
        expect 'TEST map-script-counters math frame=234 sum=16 product=64 difference=60 result=12'
    fi
    for frame in 242 246; do
        if covers "$frame"; then
            expect "TEST map-script-counters state frame=$frame copy=12 xp=20 threat=7 living=1 all=3 equal=1 greater=1 less=0"
        fi
    done
}

prepare() {
    printf '%s map-script-counters-load-levels - - 0 %s/levels.ini\n' 231 "$package_dir"
    printf '%s map-script-counters-load-levels - - 0 %s/levels.ini\n' "$save_frame" "$package_dir"
}
