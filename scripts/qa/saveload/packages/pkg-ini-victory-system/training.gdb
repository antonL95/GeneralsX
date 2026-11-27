python
import os

with open(os.environ["GENERALSX_TEST_COMMANDS"]) as commands:
    victory_fixture = any(" create Player_1 VictoryFixtureShooter " in line for line in commands)
if victory_fixture:
    gdb.execute("set environment GENERALSX_RETAIL_SKIRMISH_AI 1")
    for breakpoint in gdb.breakpoints():
        if breakpoint.location == "GameInfo::startGame":
            breakpoint.commands = "\n".join((
                "silent",
                "set variable this->m_slot[1]->m_state = SLOT_PLAYER",
                "set variable this->m_slot[1]->m_IP = 1",
                "continue",
            ))
end
