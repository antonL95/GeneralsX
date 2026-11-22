break GameState::saveGame
commands
  silent
  set $owner = ThePlayerList->m_local
  set $book = $owner->getRetailSpellBook()
  if !$book
    printf "TEST save-load spellbook was missing before removal\n"
    quit 1
  end
  set $removed = $book->m_id
  call TheGameLogic->destroyObject($book)
  call TheGameLogic->processDestroyList()
  if $owner->getRetailSpellBook()
    printf "TEST save-load spellbook removal failed\n"
    quit 1
  end
  printf "TEST save-load removed spellbook id=%u next=%u\n", $removed, TheGameLogic->m_nextObjID
  continue
end
