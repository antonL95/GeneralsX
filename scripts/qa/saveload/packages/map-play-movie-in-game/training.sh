check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST play-movie-in-game frame=$frame registered=1 stream=1 executed=1" || return 1
    done
}
