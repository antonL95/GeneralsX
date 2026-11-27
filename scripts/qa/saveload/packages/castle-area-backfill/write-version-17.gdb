break Xfer::xferVersion if currentVersion == 18 && this->m_xferMode == XFER_SAVE && $_caller_is("GameLogic::xfer")
commands
  silent
  set variable *versionData = 17
  printf "TEST castle-area-backfill legacy logic version=%u\n", *versionData
  continue
end
