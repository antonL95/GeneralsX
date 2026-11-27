prepare() {
    if ! grep -q ' terrain-claim-build Player_2 CrossClaimPorter ' "$out/commands.txt"; then
        return
    fi
    cat <<'GDB' >> "$out/start.gdb"
set environment GENERALSX_RETAIL_SKIRMISH_AI 1
tbreak GameInfo::closeOpenSlots
commands
  silent
  set variable this->m_slot[1]->m_state = SLOT_PLAYER
  set variable this->m_slot[1]->m_IP = 1
  continue
end
GDB
}

check() {
    if ! grep -q ' terrain-claim-build Player_2 CrossClaimPorter ' "$out/continue.txt"; then
        return
    fi
    for frame in 80 120; do
        if covers "$frame"; then
            expect "TEST terrain-claim build frame=$frame placed=1" || return 1
        fi
    done
    awk -v first="$first" -v end="$end" -v phase="$phase" '
    function covers(frame) { return first <= frame && frame <= end }
    function verify(ok, message) {
        if (!ok) {
            print "Terrain claim:", message, phase
            failed = 1
        }
    }
    $1 == "TEST" && $2 == "resground" {
        frame = substr($4, 7) + 0
        position = substr($5, 3) + 0
        seen[frame, position] = 1
        complete[frame, position] = substr($6, 10) + 0
        efficiency[frame, position] = substr($NF, 12) + 0
        if (position == 3300 || position == 3500)
            print $0
    }
    END {
        if (covers(60) && covers(100))
            verify(efficiency[60, 3500] > 0.6 && efficiency[100, 3500] == efficiency[60, 3500],
                "unfinished enemy farm leaves the preview available")
        if (covers(140))
            verify(seen[140, 3300] && seen[140, 3500] && !complete[140, 3300] && !complete[140, 3500] &&
                efficiency[140, 3300] > 0.9 && efficiency[140, 3500] > 0.6, "both foundations reserve the overlap")
        if (covers(420))
            verify(seen[420, 3300] && !complete[420, 3300] && complete[420, 3500] &&
                efficiency[420, 3300] > 0 && efficiency[420, 3300] < 0.5 && efficiency[420, 3500] > 0.6,
                "the first completed farm takes the overlap")
        if (covers(1350))
            verify(complete[1350, 3300] && complete[1350, 3500] &&
                efficiency[1350, 3300] > 0 && efficiency[1350, 3300] < 0.5 && efficiency[1350, 3500] > 0.6,
                "the later completion gets only the remaining cells")
        if (covers(1370))
            verify(complete[1370, 3300] && efficiency[1370, 3300] > 0.9, "destroying the owner releases its cells")
        exit failed
    }' "$log"
}
