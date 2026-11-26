prepare() {
    if grep -q ' create Player_2 SkirmishExpansionPorter ' "$out/commands.txt"; then
        cat <<'EOF' >> "$out/start.gdb"
set environment GENERALSX_RETAIL_SKIRMISH_AI brutal
set environment GENERALSX_SKIRMISH_AI_TRACE 1
tbreak GameInfo::startGame
commands
  silent
  set variable this->m_slot[0]->m_playerTemplate = -2
  set variable this->m_slot[0]->m_origPlayerTemplate = -2
  continue
end
break ScriptEngine::startEndGameTimer
commands
  silent
  return
  continue
end
EOF
        cat <<'EOF' >> "$out/resume.gdb"
break ScriptEngine::startEndGameTimer
commands
  silent
  return
  continue
end
EOF
    fi
}

check() {
    if ! grep -q ' create Player_2 SkirmishExpansionPorter ' "$out/continue.txt"; then
        return 0
    fi
    if covers 50; then
        expect 'TEST skirmish-expansion frame=50 building=0 at-start=0' || return 1
    fi
    for frame in 132 300; do
        if covers "$frame"; then
            expect "TEST skirmish-expansion frame=$frame building=1 at-start=1" || return 1
        fi
    done
    printf 'PASS expansion tactic builds fortress at unoccupied player start across reload (%s)\n' "$phase"
}
