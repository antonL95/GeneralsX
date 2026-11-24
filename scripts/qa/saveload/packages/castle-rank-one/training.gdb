python
import gdb
import os
with open(os.environ["GENERALSX_TEST_COMMANDS"], "r") as commands:
    isolated = "castle-rank-one-native" in commands.read()
if isolated:
    for breakpoint in gdb.breakpoints():
        if breakpoint.location == "GameInfo::startGame":
            breakpoint.commands = "silent\nset variable this->m_slot[1]->m_state = SLOT_CLOSED\nset variable this->m_slot[0]->m_playerTemplate = (int)findRetailSkirmishFaction(\"FactionWild\")\ncontinue"
end
