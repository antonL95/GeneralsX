prepare() {
    for script in start resume; do
        printf '%s\n' 'set environment GENERALSX_INI_TRACE_DEFINE SLOT3_COMMAND_MAP' >> "$out/$script.gdb"
    done
}

check_frame() {
    if covers "$1"; then
        expect "$2"
    fi
}

check() {
    if grep -Ei '^\[INI skipped\] (.*[\\/])?commandmap\.ini:' "$log"; then
        return 1
    fi
    grep -F "[INI] load('CommandMap.ini') END" "$log"
    grep -F "[INI] load('Data\INI\CommandMap.ini') END" "$log"
    check_frame 54 'TEST rotwk-hotkeys frame=54 attack-move=1 waypoint=1 stance=aggressive'
    check_frame 59 'TEST rotwk-hotkeys frame=59 attack-move=0 waypoint=0 stance=hold-ground'
    check_frame 62 'TEST rotwk-hotkeys frame=62 attack-move=0 waypoint=0 stance=hold-ground'
    check_frame 66 'TEST rotwk-hotkeys frame=66 attack-move=0 waypoint=0 stance=battle'
    check_frame 71 'TEST rotwk-hotkey-sale frame=71 sold=0'
    check_frame 75 'TEST rotwk-hotkey-sale frame=75 sold=1'
}
