tbreak GameLogic::loadMapINI
commands
  silent
  set $loaded = RetailINIStartup::loadSubsystem("TheFXParticleSystemManager", 0, 0, 0)
  printf "TEST map-ini retail particle definitions initialized=%d\n", $loaded
  continue
end
