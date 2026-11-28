check() {
    for frame in 242 245; do
        covers "$frame" || continue
        expect "TEST camera-entered-area frame=$frame inside=1 outside=0 multiplayer=0 replay=0 unsampled=0" || return 1
    done
}
