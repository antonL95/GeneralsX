check() {
    for frame in 236 242 246 247 249; do
        if covers "$frame"; then
            case "$frame" in
                236|249) state='first=0 overlap=0 notch=0 human=1 other=1 humanGoal=1 otherGoal=1' ;;
                242|246) state='first=1 overlap=1 notch=0 human=0 other=1 humanGoal=0 otherGoal=1' ;;
                247) state='first=0 overlap=1 notch=0 human=0 other=1 humanGoal=0 otherGoal=1' ;;
            esac
            expect "TEST map-script-shroud frame=$frame $state"
        fi
    done
}
