prepare() {
    if grep -q '^45 destroy Player_1 MenFortressCitadel 0$' "$out/commands.txt"; then
        cat >> "$out/start.gdb" <<'EOF'
set environment GENERALSX_RETAIL_SKIRMISH_AI brutal
break ScriptEngine::startEndGameTimer
commands
  silent
  printf "TEST partition-fortress-destroy suppressed end-game timer at frame=%u\n", TheGameLogic->m_frame
  return
  continue
end
EOF
        cat >> "$out/resume.gdb" <<'EOF'
break ScriptEngine::startEndGameTimer
commands
  silent
  printf "TEST partition-fortress-destroy suppressed end-game timer at frame=%u\n", TheGameLogic->m_frame
  return
  continue
end
EOF
    fi
}

check() {
    if ! grep -q '^45 destroy Player_1 MenFortressCitadel 0$' "$out/continue.txt"; then
        return 0
    fi
    if covers 45; then
        grep -q '^TEST frame 45: destroy object [0-9][0-9]* (GondorFarm)$' "$log" || {
            printf 'Missing ordinary building destroy log at frame 45 in %s\n' "$phase"
            return 1
        }
        grep -q '^TEST frame 45: destroy object [0-9][0-9]* (MenFortressCitadel)$' "$log" || {
            printf 'Missing fortress destroy log at frame 45 in %s\n' "$phase"
            return 1
        }
        expect 'TEST partition-fortress-destroy suppressed end-game timer at frame=46' || return 1
    fi
    if [ "$phase" = resume ]; then
        if grep -q '^TEST partition-fortress-destroy suppressed end-game timer at frame=' "$log"; then
            printf 'Unexpected end-game timer re-trigger after load in %s\n' "$phase"
            return 1
        fi
    fi
}
