import os
import struct

import gdb


class Draw(gdb.Breakpoint):
    last_frame = None
    saved_weather_visibility = None
    hidden_weather_frames = {int(frame) for frame in os.environ.get("PLAYTEST_HIDE_WEATHER_FRAMES", "").split()}
    render_from = int(os.environ["PLAYTEST_RENDER_FROM"])
    capture_windows = set()
    static_captures = os.environ.get("PLAYTEST_STATIC_CAPTURES")
    render_windows = tuple(
        tuple(int(frame) for frame in interval.split(":"))
        for interval in os.environ.get("PLAYTEST_RENDER_WINDOWS", "").split()
    )
    if static_captures in ("1", "exact"):
        warmup = 2 if static_captures == "1" else 0
        for capture in os.environ["PLAYTEST_FRAMES"].split():
            frame = int(capture)
            capture_windows.update(range(frame - warmup, frame + 1))

    def stop(self):
        frame = logic_frame()
        if frame < 20:
            return False
        if self.last_frame is None:
            gdb.execute("set variable TheFramePacer->m_enableLogicTimeScale = 0", to_string=True)
            gdb.execute("set variable TheFramePacer->m_enableFpsLimit = 0", to_string=True)
        skip_static_frame = self.capture_windows and frame >= 28 and frame not in self.capture_windows
        skip_window_frame = self.render_windows and frame >= 28 and not any(
            first <= frame <= last for first, last in self.render_windows
        )
        if frame == self.last_frame or 28 <= frame < self.render_from or skip_static_frame or skip_window_frame:
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


def capture_resources():
    textures = gdb.parse_and_eval("WW3DAssetManager::TheInstance->TextureHash")
    table = textures["Table"]
    stride = table.type.target().sizeof
    cached = int(textures["Size"])
    entries = gdb.selected_inferior().read_memory(int(table), cached * stride)
    slot = int(textures["First"])
    while slot != -1:
        cached -= 1
        slot = struct.unpack_from("<i", entries, slot * stride)[0]
    return int(gdb.parse_and_eval("DX8Wrapper::FrameCount")), cached, int(gdb.parse_and_eval("_VertexBufferCount"))


class Capture(gdb.Breakpoint):
    def __init__(self):
        super().__init__("UiTest::capturePath", internal=True)
        self.frames = {int(frame) for frame in os.environ["PLAYTEST_FRAMES"].split()}
        self.trace_resources = os.environ.get("PLAYTEST_TRACE_CAPTURE_RESOURCES") == "1"
        self.before = None
        if self.trace_resources:
            CaptureComplete(self)

    def stop(self):
        frame = logic_frame()
        if frame not in self.frames:
            return False
        if self.trace_resources:
            self.before = int(gdb.parse_and_eval("DX8Wrapper::FrameCount"))
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


class CaptureComplete(gdb.Breakpoint):
    def __init__(self, capture):
        super().__init__("UiTest::markCaptured", internal=True)
        self.capture = capture

    def stop(self):
        if self.capture.before is not None:
            present, textures, vertices = capture_resources()
            delta = present - self.capture.before
            gdb.write(f"CAPTURE frame={logic_frame()} present_delta={delta} textures={textures} vertex_buffers={vertices}\n")
            self.capture.before = None
        return False


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


class RestoreGamePause(gdb.FinishBreakpoint):
    def __init__(self, paused):
        super().__init__(internal=True)
        self.paused = paused

    def stop(self):
        gdb.execute(f"set variable TheGameLogic->m_gamePaused = {self.paused}", to_string=True)
        return False


class SparkleClockUpdates(gdb.Breakpoint):
    def __init__(self, first_paused_frame, last_paused_frame):
        super().__init__("ControlBar::update", internal=True)
        self.first = first_paused_frame
        self.last = last_paused_frame
        self.elapsed_ms = 0.0
        self.paused_updates = 0

    def stop(self):
        if self.first <= logic_frame() <= self.last:
            RestoreGamePause(int(gdb.parse_and_eval("TheGameLogic->m_gamePaused")))
            gdb.execute("set variable TheGameLogic->m_gamePaused = 1", to_string=True)
            self.paused_updates += 1
        else:
            self.elapsed_ms += 1000 * float(gdb.parse_and_eval("TheFramePacer->m_updateTime"))
        return False


class TraceSparkleClock(gdb.Breakpoint):
    def __init__(self, clock):
        super().__init__("AptScene::drawFrameAt", internal=True)
        self.clock = clock

    def stop(self):
        caller = gdb.newest_frame().older()
        if caller and "drawAutoAbilitySparkles" in (caller.name() or ""):
            index = int(gdb.parse_and_eval("frame"))
            gdb.write(f"TEST sparkle-clock frame={logic_frame()} clip={index} "
                      f"elapsed={self.clock.elapsed_ms:.3f} paused_updates={self.clock.paused_updates}\n")
        return False


Draw("W3DDisplay::draw", internal=True)
Capture()
if os.environ.get("PLAYTEST_PIN_DYNAMIC_LOD") == "1":
    PinDynamicLOD("GameLODManager::findDynamicLODLevel", internal=True)
if "PLAYTEST_FIXED_UPDATE_FPS" in os.environ:
    PinClientUpdateDelta(int(os.environ["PLAYTEST_FIXED_UPDATE_FPS"]))
if "PLAYTEST_SPARKLE_PAUSE_FRAMES" in os.environ:
    TraceSparkleClock(SparkleClockUpdates(*map(int, os.environ["PLAYTEST_SPARKLE_PAUSE_FRAMES"].split(":"))))
