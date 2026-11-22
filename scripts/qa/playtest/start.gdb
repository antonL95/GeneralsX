set debuginfod enabled off
set print thread-events off
set pagination off
set confirm off

tbreak GameInfo::startGame
commands
  silent
  set variable this->m_slot[1]->m_state = SLOT_CLOSED
  continue
end

break ScriptEngine::startEndGameTimer
commands
  silent
  return
  continue
end
