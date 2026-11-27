prepare() {
    if ! grep -q '^2 heroes Player_2 - 0$' "$out/commands.txt"; then
        return
    fi
    cat <<'GDB' >> "$out/start.gdb"
set environment GENERALSX_SKIRMISH_PROBE_A FactionIsengard
set environment GENERALSX_SKIRMISH_PROBE_B FactionMordor
set environment GENERALSX_SKIRMISH_PROBE_DIFFICULTY hard
set environment GENERALSX_SKIRMISH_PROBE_FRAMES 3400
set environment GENERALSX_SKIRMISH_AI_TRACE 1
tbreak GameInfo::startGame
commands
  silent
  set variable this->m_startingCash.m_money = 4000
  set variable this->m_slot[0]->m_state = SLOT_PLAYER
  set variable this->m_localIP = this->m_slot[0]->m_IP
  set variable this->m_slot[2]->m_state = SLOT_CLOSED
  continue
end
GDB
    printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
    awk -v end="$end" 'BEGIN {
        for (frame = 90; frame <= end && frame <= 1800; frame += 15) {
            print frame, "log Player_2 MordorOrcPit 0"
            print frame, "log Player_2 MordorSlaughterHouse_ForAI 0"
        }
    }'
}

check() {
    if ! grep -q '^2 heroes Player_2 - 0$' "$out/continue.txt"; then
        return
    fi
    if covers 2; then
        grep -E '^TEST heroes 2 Player_2 money=4000 count=[0-9]+$' "$log" || return 1
        awk -F '\t' '$1 == "AI_BUILD" && $3 == 4 {
            print "Opening order", $2, $5
            if (++orders == 1 && $5 == "MordorOrcPit") first = $2
            if (orders == 2 && $5 == "MordorSlaughterHouse_ForAI") second = $2
            if (orders == 2) exit
        }
        END { exit !(first && second > first && second - first <= 180) }' "$log" || return 1
    fi
    if covers 1800; then
        awk -v phase="$phase" 'BEGIN { pit = "MordorOrcPit"; farm = "MordorSlaughterHouse_ForAI" }
        $1 == "TEST" && $2 == "frame" && $4 == "object" && $(NF-2) == 0 && ($6 == pit || $6 == farm) {
            frame = $3 + 0
            progress[$6, frame] = $NF + 0
            if ($NF < 0 && !complete[$6]) complete[$6] = frame
            if (!overlap && progress[pit, frame] > 0 && progress[farm, frame] > 0) overlap = frame
        }
        END {
            print "Opening overlap", overlap, "farm complete", complete[farm], "stronghold complete", complete[pit], phase
            exit !(overlap && complete[farm] > overlap && complete[pit] > complete[farm])
        }' "$log" || return 1
    fi
    if covers 3300; then
        awk -F '\t' '$1 == "AI_TRAIN" && $3 == 4 &&
            $4 ~ /^Command_ConstructMordor(OrcFighter|Pike|OrcArcher|BlackOrcFighter)Horde$/ {
            print "Opening infantry order", $2, $5
            found = 1
            exit
        }
        END { exit !found }' "$log" || return 1
        awk '$1 == "TEST" && $2 == "all" && $3 == 3300 && $10 == 4 && $12 == 0 &&
            $5 ~ /^Mordor(Fighter|Pikeman|Archer|BlackOrc)Horde(_AI)?$/ { found = 1 }
            END { exit !found }' "$log" || return 1
    fi
}
