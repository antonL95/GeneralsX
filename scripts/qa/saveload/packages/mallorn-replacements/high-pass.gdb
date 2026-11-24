python
import os
if os.environ.get("MALLORN_FIXTURE") == "1":
    start = gdb.Breakpoint("GameInfo::startGame", temporary=True)
    start.commands = '\n'.join((
        "silent",
        'set variable this->m_slot[1]->m_playerTemplate = (int)findRetailSkirmishFaction("FactionElves")',
        "continue",
    ))
end
