set environment GENERALSX_RETAIL_SKIRMISH_AI brutal

tbreak GameInfo::closeOpenSlots
commands
    silent
    set variable this->m_slot[1]->m_state = SLOT_RETAIL_BRUTAL_AI
    continue
end
