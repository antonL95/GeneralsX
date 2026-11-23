import ctypes as c
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time
import zlib


def bind(library, name, result, *args):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = args
    return function


x11 = c.CDLL("libX11.so.6")
xtst = c.CDLL("libXtst.so.6")
display = bind(x11, "XOpenDisplay", c.c_void_p, c.c_char_p)(None)
if not display:
    sys.exit("Could not open the test X11 display")
root = bind(x11, "XDefaultRootWindow", c.c_ulong, c.c_void_p)(display)
sync = bind(x11, "XSync", c.c_int, c.c_void_p, c.c_int)
get_image = bind(x11, "XGetImage", c.c_void_p, c.c_void_p, c.c_ulong,
                 c.c_int, c.c_int, c.c_uint, c.c_uint, c.c_ulong, c.c_int)
get_pixel = bind(x11, "XGetPixel", c.c_ulong, c.c_void_p, c.c_int, c.c_int)
destroy_image = bind(x11, "XDestroyImage", c.c_int, c.c_void_p)
motion = bind(xtst, "XTestFakeMotionEvent", c.c_int, c.c_void_p, c.c_int,
              c.c_int, c.c_int, c.c_ulong)
button = bind(xtst, "XTestFakeButtonEvent", c.c_int, c.c_void_p, c.c_uint,
              c.c_int, c.c_ulong)
key_event = bind(xtst, "XTestFakeKeyEvent", c.c_int, c.c_void_p, c.c_uint,
                 c.c_int, c.c_ulong)
keycode = bind(x11, "XKeysymToKeycode", c.c_uint, c.c_void_p, c.c_ulong)


def key(value):
    code = keycode(display, value)
    key_event(display, code, 1, 0)
    key_event(display, code, 0, 80)
    sync(display, 0)


def click(x, y, settle=0.8):
    motion(display, 0, x, y, 0)
    sync(display, 0)
    time.sleep(0.12)
    button(display, 1, 1, 0)
    button(display, 1, 0, 80)
    sync(display, 0)
    time.sleep(settle)


def pixel(x, y):
    image = get_image(display, root, x, y, 1, 1, 0xFFFFFFFF, 2)
    value = get_pixel(image, 0, 0)
    destroy_image(image)
    return value


def capture(name, x, y, width, height):
    image = get_image(display, root, x, y, width, height, 0xFFFFFFFF, 2)
    rows = bytearray()
    for row in range(height):
        rows.append(0)
        for col in range(width):
            value = get_pixel(image, col, row)
            rows.extend(value.to_bytes(4, "big")[1:])
    destroy_image(image)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    (out / (name + ".png")).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def wait_for(label, predicate, timeout=30, interval=0.2):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            print(f"PASS {label}", flush=True)
            return
        if game.poll() is not None:
            break
        time.sleep(interval)
    capture("failed", 0, 0, 1024, 768)
    print("\n".join((out / "game.log").read_text(errors="replace").splitlines()[-12:]), flush=True)
    sys.exit(f"FAIL {label}")


def log_count(message):
    return (out / "game.log").read_text(errors="replace").count(message)


def faction_history():
    log = (out / "game.log").read_text(errors="replace")
    return re.findall(r"TEST player 30 Player_1 template=(\S+)", log)


def start_match(faction_row):
    click(512, 296)
    click(360, 444)
    click(360, 169 + 25 * faction_row)
    click(170, 465)
    click(170, 541)
    click(833, 689)


def menu_visible():
    return pixel(365, 318) == 0x14212A and pixel(366, 319) == 0x14212A


def title_signature(x, y, width, height):
    image = get_image(display, root, x, y, width, height, 0xFFFFFFFF, 2)
    try:
        return tuple(get_pixel(image, col, row)
                     for row in range(0, height, 3) for col in range(0, width, 3))
    finally:
        destroy_image(image)


def skirmish_label_pixels():
    return {
        index for index, color in enumerate(title_signature(452, 281, 120, 32))
        if min(color.to_bytes(4, "big")[1:]) > 150
    }


def power_points():
    log = (out / "game.log").read_text(errors="replace")
    return [int(points) for points in re.findall(r"TEST powerstate \d+ Player_1 .*? points=(\d+)", log)]


def spellbook_flames():
    return {
        index for index, color in enumerate(title_signature(352, 188, 69, 22))
        if (color >> 8 & 0xFF) > 165 and (color & 0xFF) > 170
        and (color & 0xFF) > (color >> 16 & 0xFF) + 40
    }


def owned_check_visible():
    expected = (154, 202, 164)
    image = get_image(display, root, 371, 257, 30, 36, 0xFFFFFFFF, 2)
    try:
        columns = [
            x for x in range(30) for y in range(36)
            if all(abs(actual - target) <= 3
                   for actual, target in zip(get_pixel(image, x, y).to_bytes(4, "big")[1:], expected))
        ]
        return bool(columns) and max(columns) - min(columns) >= 10
    finally:
        destroy_image(image)


def connector_brightness():
    signature = title_signature(337, 274, 48, 54)
    return sum(color & 0xFF for color in signature) / len(signature)


def check_spellbook():
    arena = title_signature(380, 110, 264, 72)
    click(128, 553, settle=0)
    wait_for("the spellbook opening becomes visible",
             lambda: sum(abs((before & 0xFF) - (after & 0xFF)) > 24
                         for before, after in zip(arena, title_signature(380, 110, 264, 72)))
                         > len(arena) // 4, interval=0.02)
    capture("spellbook-opening", 247, 65, 530, 340)
    time.sleep(0.65)
    observations = len(power_points())
    click(320, 362)
    click(568, 684)
    wait_for("unavailable power leaves fifteen points unspent",
             lambda: len(power_points()) > observations and power_points()[-1] == 15, 60)
    capture("spellbook-available", 285, 185, 455, 201)
    wait_for("available power shows flames", spellbook_flames, 4)
    initial_flames = spellbook_flames()
    wait_for("available power flames animate", lambda: spellbook_flames() != initial_flames, 4)
    baseline_connector = connector_brightness()
    click(386, 246)
    click(568, 684)
    wait_for("available power can be purchased for five points", lambda: power_points()[-1] == 10, 60)
    wait_for("purchased power displays its check mark", owned_check_visible)
    wait_for("purchased power fills its outgoing connector",
             lambda: connector_brightness() > baseline_connector + 8)
    key(0xFF1B)
    wait_for("Escape closes the spellbook", lambda: not owned_check_visible() and not menu_visible())
    click(128, 553)
    wait_for("purchased power keeps its check mark after reopening", owned_check_visible)
    capture("spellbook-purchased", 285, 185, 455, 201)
    key(0xFF1B)
    wait_for("the spellbook closes again", lambda: not owned_check_visible() and not menu_visible())
    right = keycode(display, 0xFF53)
    key_event(display, right, 1, 0)
    sync(display, 0)
    try:
        click(128, 553)
        wait_for("the spellbook opens with the camera key held", owned_check_visible)
        key(0xFF1B)
        wait_for("Escape closes the spellbook with the camera key held",
                 lambda: not owned_check_visible() and not menu_visible())
    finally:
        key_event(display, right, 0, 0)
        sync(display, 0)


def check():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    skirmish_label = skirmish_label_pixels()
    if len(skirmish_label) < 10:
        sys.exit("FAIL initial shell has no visible Skirmish label")
    start_match(1)
    wait_for("first skirmish is running", lambda: len(faction_history()) == 1, 100)
    wait_for("first skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 1, 60)
    if "--spellbook" in sys.argv:
        check_spellbook()
        return
    key(0xFF1B)
    wait_for("Escape opens the in-game menu", menu_visible)
    capture("pause-menu", 360, 312, 304, 170)
    progress = log_count("TEST powerstate ")
    time.sleep(3)
    if log_count("TEST powerstate ") != progress:
        sys.exit("FAIL simulation advances while the match menu is open")
    print("PASS the match is paused", flush=True)
    click(512, 391)
    wait_for("Resume dismisses the menu", lambda: not menu_visible())
    wait_for("the same match advances again",
             lambda: len(faction_history()) == 1 and log_count("TEST powerstate ") > progress, 45)
    key(0xFF1B)
    wait_for("Escape reopens the menu in the same match", menu_visible)
    pause_title = title_signature(376, 323, 272, 35)
    click(512, 442)
    wait_for("Leave opens the confirmation", lambda: title_signature(376, 323, 272, 35) != pause_title)
    capture("leave-confirmation", 360, 312, 304, 170)
    key(0xFF1B)
    wait_for("Escape cancels the leave confirmation",
             lambda: title_signature(376, 323, 272, 35) == pause_title)
    click(512, 442)
    wait_for("Leave opens the confirmation again", lambda: title_signature(376, 323, 272, 35) != pause_title)
    click(512, 442)
    wait_for("repeating the Leave click returns to the pause menu",
             lambda: title_signature(376, 323, 272, 35) == pause_title)
    click(512, 442)
    wait_for("Leave reopens the confirmation", lambda: title_signature(376, 323, 272, 35) != pause_title)
    click(512, 391)
    wait_for("Quit returns to the main menu",
             lambda: len(skirmish_label & skirmish_label_pixels()) >= len(skirmish_label) * 0.8)
    capture("main-menu-return", 425, 265, 174, 170)
    start_match(2)
    wait_for("a second skirmish runs with a different faction",
             lambda: len(faction_history()) == 2 and len(set(faction_history())) == 2, 100)
    wait_for("second skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 2, 60)
    print("Factions: " + ", ".join(faction_history()), flush=True)
    key(0xFF1B)
    wait_for("Escape opens the in-game menu in the second match", menu_visible)


build = Path(sys.argv[1]).resolve()
out = Path(tempfile.mkdtemp(prefix="pause-qa.", dir=build))
print(f"Run directory {out}", flush=True)
runtime = out / "runtime"
runtime.mkdir(mode=0o700)
commands = out / "commands.txt"
commands.write_text(("15 pkg44-points Player_1 - 0 10\n" if "--spellbook" in sys.argv else "") +
                    "30 player Player_1 - 0\n" + "".join(
    f"{frame} powerstate Player_1 - 0\n" for frame in range(60, 3600, 30)
))
env = os.environ | {
    "XDG_DATA_HOME": str(out / "data"),
    "XDG_CONFIG_HOME": str(out / "config"),
    "XDG_CACHE_HOME": str(out / "cache"),
    "XDG_RUNTIME_DIR": str(runtime),
    "DXVK_LOG_PATH": str(out),
    "DXVK_STATE_CACHE_PATH": str(out / "cache"),
    "LD_LIBRARY_PATH": ":".join(sorted({str(lib.parent) for lib in build.rglob("*.so*")})),
    "VK_DRIVER_FILES": str(next(Path("/usr/share/vulkan/icd.d").glob("lvp*.json"))),
    "SDL_VIDEO_DRIVER": "x11",
    "SDL_AUDIO_DRIVER": "dummy",
    "LP_NUM_THREADS": "3",
    "GENERALSX_TEST_COMMANDS": str(commands),
}
with (out / "game.log").open("wb") as log:
    game = subprocess.Popen(
        [str(build / "GeneralsMD/GeneralsXZH"), "-win", "-xres", "1024", "-yres", "768",
         "-noaudio", "-nomusic", "-novideo", "-noshellmap",
         "-mod", "/game-install/aotr", "-bfme2", "/game-install/bfme2"],
        cwd="/game-install/rotwk", env=env, stdout=log, stderr=subprocess.STDOUT,
    )
    try:
        check()
    finally:
        game.terminate()
        try:
            game.wait(timeout=10)
        except subprocess.TimeoutExpired:
            game.kill()
            game.wait()
