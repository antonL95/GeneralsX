check() {
    result=0
    for frame in 242 245; do
        if covers "$frame"; then
            expect "TEST script audio frame=$frame named=1 mapped=1" || result=1
            grep -E "^AUDIO TRACE frame=$frame kind=sound-queue owner=[1-9][0-9]* handle=[0-9]+ event=AudioScriptFixtureSound$" "$log" || {
                printf 'Missing named sound request in %s at frame %s\n' "$phase" "$frame" >&2
                result=1
            }
        fi
    done
    for frame in 243 246; do
        if covers "$frame"; then
            expect "TEST script audio volume frame=$frame mapped=1 muted=1" || result=1
        fi
    done
    if covers 247; then
        owner=$(sed -n 's/^TEST script audio team frame=247 mapped=1 owner=\([1-9][0-9]*\)$/\1/p' "$log")
        if [ -z "$owner" ] || ! grep -E "^AUDIO TRACE frame=247 kind=sound-queue owner=$owner handle=[0-9]+ event=AudioScriptFixtureSound$" "$log"; then
            printf 'Missing team sound request in %s at frame 247\n' "$phase" >&2
            result=1
        fi
    fi
    for assertion in '238 room'; do
        frame=${assertion% *}
        kind=${assertion#* }
        if covers "$frame"; then
            expect "TEST script audio $kind frame=$frame mapped=1" || result=1
        fi
    done
    return "$result"
}
