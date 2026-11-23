python
import os
queue_case = os.environ.get("FOLLOWUP_QUEUE_CASE")
if queue_case:
    faction = "FactionArnor" if queue_case in ("archer-first", "heroes-first") else "FactionMen"
    gdb.execute("set confirm off")
    start = gdb.Breakpoint("GameInfo::startGame", temporary=True)
    start.commands = "\n".join((
        "silent",
        "set variable this->m_slot[1]->m_state = SLOT_CLOSED",
        f'set variable this->m_slot[0]->m_playerTemplate = (int)findRetailSkirmishFaction("{faction}")',
        "continue",
    ))
    victory = gdb.Breakpoint("ScriptEngine::startEndGameTimer")
    victory.commands = "silent\nreturn\ncontinue"
end
