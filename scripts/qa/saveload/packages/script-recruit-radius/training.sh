check() {
    for frame in 242 247; do
        if covers "$frame"; then
            expect "TEST script-recruit-radius frame=$frame radius=400 anchor=1 near=1 far=0 emptyFar=1 farFromHome=1"
        fi
    done
}
