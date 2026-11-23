set $cleared_replay = 0
break GameLogic::clearGameData if this->m_gameMode == GAME_REPLAY
commands
  silent
  printf "TEST followup replay cleared at frame %u\n", this->m_frame
  set $cleared_replay = 1
  continue
end
tbreak drawReturnMenu if $cleared_replay
commands
  silent
  call TheWindowManager->winSendInputMsg(window, GWM_CHAR, KEY_ESC, KEY_STATE_DOWN)
  printf "TEST followup replay return menu drew and received Escape\n"
  continue
end
