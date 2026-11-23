set environment GENERALSX_RETAIL_SKIRMISH_AI brutal
set environment GENERALSX_SKIRMISH_AI_TRACE 1

tbreak GameInfo::closeOpenSlots
commands
    silent
    set variable this->m_slot[1]->m_state = SLOT_RETAIL_BRUTAL_AI
    continue
end
