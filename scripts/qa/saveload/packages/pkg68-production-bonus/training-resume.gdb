set environment GENERALSX_TRACE_RETAIL_AUDIO 1

break RetailTestCommands::runFrame if frame == 211
commands
  silent
  set $civilian_key = TheNameKeyGenerator->nameToKey((const char *)"PlyrCivilian")
  set $civilian = ThePlayerList->findPlayerWithNameKey($civilian_key)
  if $civilian
    set variable $civilian->m_canBuildUnits = 1
  end
  continue
end
