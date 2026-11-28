python
import os
with open(os.path.join(os.path.dirname(os.environ["GENERALSX_TEST_COMMANDS"]), "continue.txt")) as commands:
    objectives_tribute_active = any(" hud-tribute-setup " in line for line in commands)
if objectives_tribute_active:
    victory = gdb.Breakpoint("VictoryConditions::update")
    victory.silent = True
    victory.commands = "\n".join(("return", "continue"))
end
