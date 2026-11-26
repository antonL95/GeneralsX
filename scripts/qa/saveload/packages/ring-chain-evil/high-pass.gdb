python
import os
with open(os.environ["GENERALSX_TEST_COMMANDS"]) as commands:
    ring_evil_active = any(" ringchain-gollum Player_1 RingEvilGollum " in line for line in commands)
if ring_evil_active:
    start = gdb.Breakpoint("GameInfo::startGame", temporary=True)
    start.commands = '\n'.join((
        "silent",
        'set variable this->m_slot[0]->m_playerTemplate = (int)findRetailSkirmishFaction("FactionMordor")',
        "continue",
    ))
end
