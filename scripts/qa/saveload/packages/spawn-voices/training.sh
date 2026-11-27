check_voices() {
    awk -v first="$1" -v last="$2" -v spawn="$3" '
        $1 == "AUDIO" && $2 == "TRACE" && ($4 == "kind=voice-queue" || $4 == "kind=start") {
            frame = substr($3, 7) + 0
            if (frame < first || frame > last)
                next
            if ($7 == "event=SpawnVoicesCreated" || $7 == "event=" spawn)
                print
            if ($7 == "event=SpawnVoicesCreated" && $5 != "owner=0") {
                if ($4 == "kind=voice-queue") {
                    created++
                    createdLine = NR
                } else
                    createdStarted++
            }
            if ($7 == "event=" spawn && $5 == "owner=0") {
                if ($4 == "kind=voice-queue") {
                    fx++
                    fxLine = NR
                } else
                    fxStarted++
            }
        }
        END {
            if (created != 1 || fx != 1 || createdLine >= fxLine) {
                printf "Expected one owned VoiceCreated before one positional %s in frames %d to %d\n", spawn, first, last
                exit 1
            }
            if (createdStarted != 1 || fxStarted != 1) {
                printf "Expected both voices to start in frames %d to %d\n", first, last
                exit 1
            }
        }
    ' "$log"
}

check() {
    if covers 225; then
        check_voices 184 225 SpawnVoicesInitial
    fi
    if covers 290; then
        check_voices 246 290 SpawnVoicesRevived
    fi
}
