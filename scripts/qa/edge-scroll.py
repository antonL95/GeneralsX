import ctypes
import os

import gdb


class Pointer:
    def __init__(self):
        self.x11 = ctypes.CDLL("libX11.so.6")
        display = ctypes.c_void_p
        window = ctypes.c_ulong
        integer = ctypes.c_int
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XOpenDisplay.restype = display
        self.x11.XDefaultRootWindow.argtypes = [display]
        self.x11.XDefaultRootWindow.restype = window
        self.x11.XQueryPointer.argtypes = [display, window, ctypes.POINTER(window),
                                          ctypes.POINTER(window), *[ctypes.POINTER(integer)] * 4,
                                          ctypes.POINTER(ctypes.c_uint)]
        self.x11.XSetInputFocus.argtypes = [display, window, integer, window]
        self.x11.XWarpPointer.argtypes = [display, window, window, integer, integer,
                                         ctypes.c_uint, ctypes.c_uint, integer, integer]
        self.x11.XSync.argtypes = [display, integer]
        self.display = self.x11.XOpenDisplay(None)
        if not self.display:
            raise gdb.GdbError("Could not open the Xvfb display")
        self.root = self.x11.XDefaultRootWindow(self.display)

    def move(self, x, y):
        self.x11.XWarpPointer(self.display, 0, self.root, 0, 0, 0, 0, x, y)
        self.x11.XSync(self.display, 0)

    def focus(self):
        root = ctypes.c_ulong()
        child = ctypes.c_ulong()
        coords = [ctypes.c_int() for _ in range(4)]
        mask = ctypes.c_uint()
        self.x11.XQueryPointer(self.display, self.root, ctypes.byref(root), ctypes.byref(child),
                                *map(ctypes.byref, coords), ctypes.byref(mask))
        if child.value == 0:
            raise gdb.GdbError("Expected the game window under the pointer")
        self.x11.XSetInputFocus(self.display, child, 2, 0)
        self.x11.XSync(self.display, 0)


class EdgeScroll(gdb.Breakpoint):
    def __init__(self):
        super().__init__("GameEngine::update()", internal=True)
        self.fps = int(os.environ["EDGE_SCROLL_FPS"])
        self.width = int(os.environ["EDGE_SCROLL_WIDTH"])
        self.height = int(os.environ["EDGE_SCROLL_HEIGHT"])
        self.client_frame = None
        self.samples = {}

    def stop(self):
        logic = gdb.parse_and_eval("TheGameLogic")
        if not int(logic) or int(logic["m_frame"]) < 35:
            return False
        if self.client_frame is None:
            self.pointer = Pointer()
            self.pointer.move(self.width // 2, self.height // 2)
            self.pointer.focus()
            self.client_frame = 0
        frame = self.client_frame
        self.client_frame += 1
        gdb.execute(f"set variable TheFramePacer->m_updateTime = 1.0f / {self.fps}", to_string=True)

        start = 12
        end = start + self.fps
        corner_start = end + 12
        corner_end = corner_start + self.fps
        if frame == start - 2:
            self.pointer.move(self.width - 1, self.height // 2)
        elif frame == end:
            self.pointer.move(self.width // 2, self.height // 2)
        elif frame == corner_start - 2:
            self.pointer.move(self.width - 1, 0)
        elif frame == corner_end:
            self.pointer.move(self.width // 2, self.height // 2)

        view = gdb.parse_and_eval("TheTacticalView")
        x, y = (float(view["m_pos"][axis]) for axis in ("x", "y"))
        for name, first, last in (("right", start, end), ("upper-right", corner_start, corner_end)):
            if first <= frame <= last:
                gdb.write(f"EDGE_SCROLL_FRAME {name} {frame - first} {(frame - first) / self.fps:.6f} "
                          f"{x:.6f} {y:.6f}\n")
                if frame == first:
                    self.samples[name] = x, y
                if frame == last:
                    start_x, start_y = self.samples[name]
                    gdb.write(f"EDGE_SCROLL_RESULT {self.fps} {self.width}x{self.height} {name} "
                              f"{x - start_x:.6f} {y - start_y:.6f}\n")
        if frame == corner_end + 2:
            gdb.write("EDGE_SCROLL_COMPLETE\n")
            gdb.execute("set variable TheGameEngine->m_quitting = true", to_string=True)
            self.enabled = False
        return False


EdgeScroll()
