tbreak GameLogic::update if this->m_frame == 10 && this->m_gameMode == GAME_SKIRMISH
commands
  silent
  call this->exitGame()
  continue
end
