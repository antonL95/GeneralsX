import os

import gdb


class Draw(gdb.Breakpoint):
    last_frame = None
    render_from = int(os.environ["PLAYTEST_RENDER_FROM"])

    def stop(self):
        frame = logic_frame()
        if frame < 20:
            return False
        if self.last_frame is None:
            gdb.execute("set variable TheFramePacer->m_enableLogicTimeScale = 0", to_string=True)
            gdb.execute("set variable TheFramePacer->m_enableFpsLimit = 0", to_string=True)
        if frame == self.last_frame or 28 <= frame < self.render_from:
            gdb.execute("return", to_string=True)
        self.last_frame = frame
        return False


def logic_frame():
    logic = gdb.parse_and_eval("TheGameLogic")
    return int(logic["m_frame"]) if int(logic) else 0


class Capture(gdb.Breakpoint):
    def __init__(self):
        super().__init__("UiTest::capturePath", internal=True)
        self.frames = {int(frame) for frame in os.environ["PLAYTEST_FRAMES"].split()}

    def stop(self):
        frame = logic_frame()
        if frame not in self.frames:
            return False
        prefix = f"PLAYTEST_PNG_{frame}="
        entry = gdb.parse_and_eval("(char **) __environ")
        while int(entry.dereference()):
            if entry.dereference().string(errors="replace").startswith(prefix):
                gdb.set_convenience_variable("playtest_png", entry.dereference() + len(prefix))
                gdb.execute("return (const char *) $playtest_png", to_string=True)
                self.frames.remove(frame)
                return False
            entry += 1
        raise gdb.GdbError(f"Missing capture path for frame {frame}")


Draw("W3DDisplay::draw", internal=True)
Capture()
