import os

import gdb


class Draw(gdb.Breakpoint):
    last_frame = None
    saved_weather_visibility = None
    hidden_weather_frames = {int(frame) for frame in os.environ.get("PLAYTEST_HIDE_WEATHER_FRAMES", "").split()}
    render_from = int(os.environ["PLAYTEST_RENDER_FROM"])
    capture_windows = set()
    if os.environ.get("PLAYTEST_STATIC_CAPTURES") == "1":
        for capture in os.environ["PLAYTEST_FRAMES"].split():
            frame = int(capture)
            capture_windows.update(range(frame - 2, frame + 1))

    def stop(self):
        frame = logic_frame()
        if frame < 20:
            return False
        if self.last_frame is None:
            gdb.execute("set variable TheFramePacer->m_enableLogicTimeScale = 0", to_string=True)
            gdb.execute("set variable TheFramePacer->m_enableFpsLimit = 0", to_string=True)
        skip_static_frame = self.capture_windows and frame >= 28 and frame not in self.capture_windows
        if frame == self.last_frame or 28 <= frame < self.render_from or skip_static_frame:
            gdb.execute("return", to_string=True)
        elif self.hidden_weather_frames:
            hide_weather = frame in self.hidden_weather_frames
            if hide_weather and self.saved_weather_visibility is None:
                self.saved_weather_visibility = int(gdb.parse_and_eval("TheSnowManager->m_isVisible"))
                gdb.execute("set variable TheSnowManager->m_isVisible = 0", to_string=True)
            elif not hide_weather and self.saved_weather_visibility is not None:
                gdb.execute(f"set variable TheSnowManager->m_isVisible = {self.saved_weather_visibility}", to_string=True)
                self.saved_weather_visibility = None
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


class PinDynamicLOD(gdb.Breakpoint):
    checked_mask = False

    def stop(self):
        if not self.checked_mask:
            mask = gdb.parse_and_eval(
                "TheGameLODManager->m_dynamicGameLODInfo[DYNAMIC_GAME_LOD_VERY_HIGH].m_dynamicParticleSkipMask"
            )
            if int(mask) != 0:
                raise gdb.GdbError(f"VeryHigh has particle skip mask {mask}, expected zero")
            gdb.write("TEST dynamic-lod-pinned VeryHigh skip-mask=0\n")
            self.checked_mask = True
        gdb.execute("return DYNAMIC_GAME_LOD_VERY_HIGH", to_string=True)
        return False


class PinClientUpdateDelta(gdb.Breakpoint):
    def __init__(self, fps):
        super().__init__("GameClient::update", internal=True)
        self.delta = 1.0 / fps

    def stop(self):
        gdb.execute(f"set variable TheFramePacer->m_updateTime = {self.delta:.12g}", to_string=True)
        return False


Draw("W3DDisplay::draw", internal=True)
Capture()
if os.environ.get("PLAYTEST_PIN_DYNAMIC_LOD") == "1":
    PinDynamicLOD("GameLODManager::findDynamicLODLevel", internal=True)
if "PLAYTEST_FIXED_UPDATE_FPS" in os.environ:
    PinClientUpdateDelta(int(os.environ["PLAYTEST_FIXED_UPDATE_FPS"]))
