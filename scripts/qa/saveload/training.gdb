set debuginfod enabled off
set print thread-events off
set pagination off
set confirm off

tbreak GameInfo::startGame
commands
  silent
  set variable this->m_slot[1]->m_state = SLOT_CLOSED
  printf "TEST save-load fixture closed the direct launch AI slot\n"
  continue
end

break ScriptEngine::startEndGameTimer
commands
  silent
  printf "TEST save-load fixture suppressed the port sandbox victory timer at frame=%u\n", TheGameLogic->m_frame
  return
  continue
end
