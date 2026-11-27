check() {
    for frame in 242 245; do
        if covers "$frame"; then
            expect "TEST script audio frame=$frame named=1 mapped=1"
            grep -E "^AUDIO TRACE frame=$frame kind=sound-queue owner=[1-9][0-9]* handle=[0-9]+ event=AudioScriptFixtureSound$" "$log" || {
                printf 'Missing named sound request in %s at frame %s\n' "$phase" "$frame" >&2
                return 1
            }
        fi
    done
    for frame in 243 246; do
        if covers "$frame"; then
            expect "TEST script audio volume frame=$frame mapped=1 muted=1"
        fi
    done
}
