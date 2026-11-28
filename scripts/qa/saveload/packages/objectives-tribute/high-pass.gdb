python
import os
with open(os.environ["GENERALSX_TEST_COMMANDS"]) as commands:
    objectives_tribute_active = any(" hud-tribute-setup " in line for line in commands)
if objectives_tribute_active:
    gdb.execute("set environment GENERALSX_RETAIL_SKIRMISH_AI 1")
    start = gdb.Breakpoint("GameInfo::startGame", temporary=True)
    start.commands = "\n".join((
        "silent",
        "set variable this->m_slot[1]->m_state = SLOT_PLAYER",
        "set variable this->m_slot[1]->m_IP = 1",
        "continue",
    ))
    victory = gdb.Breakpoint("VictoryConditions::update")
    victory.silent = True
    victory.commands = "\n".join(("return", "continue"))
end
