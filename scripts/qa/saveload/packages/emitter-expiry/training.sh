emitter_health() {
    grep -E "^TEST health $1 Player_1 BreezeofManwe 0 id=[0-9]+ health=$2 max=200000[.]00 dead=$3 pos=589[.]3,573[.]7$" "$log" || {
        printf 'Unexpected emitter health at frame %s in %s\n' "$1" "$phase" >&2
        return 1
    }
}

check() {
    if covers 244; then
        emitter_health 244 '200000[.]00' 0
    fi
    if covers 435; then
        emitter_health 435 '200000[.]00' 0
        awk '
            /^TEST health 435 PlyrCreeps SpiderLair 1 id=[0-9]+ health=[0-9.]+ max=2000[.]00 dead=0 pos=589[.]3,573[.]7$/ {
                split($8, health, "=")
                if (health[2] > 0 && health[2] < 2000) found = 1
            }
            END {
                if (!found) {
                    print "Expected a damaged, living Spider Lair at frame 435"
                    exit 1
                }
            }
        ' "$log"
    fi
    if covers 436; then
        emitter_health 436 '0[.]00' 1
    fi
}
