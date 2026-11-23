set debuginfod enabled off
set print thread-events off
set pagination off
set confirm off

tbreak GameInfo::startGame
commands
  silent
  set variable this->m_slot[1]->m_state = SLOT_CLOSED
  python
import json
import os

faction = os.environ.get("PLAYTEST_FACTION")
if faction:
    index = int(gdb.parse_and_eval(f"(int)findRetailSkirmishFaction({json.dumps(faction)})"))
    gdb.execute(f"set variable this->m_slot[0]->m_playerTemplate = {index}")
  end
  continue
end

break ScriptEngine::startEndGameTimer
commands
  silent
  return
  continue
end
