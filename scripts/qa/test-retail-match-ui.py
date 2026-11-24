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
get_focus = bind(x11, "XGetInputFocus", c.c_int, c.c_void_p, c.POINTER(c.c_ulong), c.POINTER(c.c_int))
move_window = bind(x11, "XMoveWindow", c.c_int, c.c_void_p, c.c_ulong, c.c_int, c.c_int)


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


def check_hud_actions():
    area = (436, 421, 154, 39)
    baseline = title_signature(*area)
    click(171, 559)
    wait_for("the Palantir banner opens Player Status",
             lambda: sum(abs((before & 0xFF) - (after & 0xFF)) > 24
                         for before, after in zip(baseline, title_signature(*area))) > len(baseline) // 3)
    visible = title_signature(*area)
    count = log_count("TEST powerstate ")
    wait_for("Player Status leaves the match running", lambda: log_count("TEST powerstate ") > count)
    capture("player-status", 240, 75, 550, 395)
    click(514, 440)
    wait_for("Cancel closes Player Status",
             lambda: sum(abs((before & 0xFF) - (after & 0xFF)) > 24
                         for before, after in zip(visible, title_signature(*area))) > len(visible) // 3)
    click(85, 559)
    wait_for("the Palantir key opens Pause", menu_visible)
    capture("palantir-pause", 360, 312, 304, 170)


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


def check_shell_load():
    global game
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    wait_for("the game window can be positioned for input", move_game_to_origin)
    click(512, 520)
    wait_for("empty Load page opens", lambda: pixel(145, 130) == 0x233A48, 5)
    capture("empty-load", 122, 110, 780, 548)
    key(0xFF0D)
    wait_for("Enter leaves the empty list open", lambda: pixel(145, 130) == 0x233A48)
    key(0xFF1B)
    wait_for("Escape returns to the main menu", lambda: len(skirmish_label_pixels()) >= 10)

    click(512, 296)
    click(219, 262)
    click(410, 210)
    click(219, 210)
    click(290, 162)
    click(454, 226)
    click(454, 352)
    click(159, 162)
    click(360, 444)
    click(360, 194)
    click(170, 465)
    click(170, 541)
    click(360, 468)
    click(360, 243)
    click(643, 468)
    click(643, 543)
    click(510, 444)
    click(510, 516)
    click(510, 468)
    click(510, 516)
    capture("skirmish-setup", 101, 137, 822, 492)
    click(833, 689)
    wait_for("match reports both occupied slots", lambda: log_count("TEST shell-slot ") == 2, 100)
    log = (out / "game.log").read_text(errors="replace")
    slots = re.findall(r"^TEST shell-slot (\d+) (\S+) team=(\d+) start=(\d+) color=(\d+) easy=(\d+)$",
                       log, re.MULTILINE)
    actual = [slot[:4] + slot[5:] for slot in slots]
    expected = [("0", "FactionMen", "0", "1", "0"), ("1", "FactionRohan", "1", "0", "1")]
    if actual != expected or len({slot[4] for slot in slots}) != 2:
        sys.exit(f"FAIL wrong skirmish roster: {slots}")
    wait_for("chosen skirmish settings reach the match",
             lambda: log_count("TEST shell-setup frame=30 cash=1400") == 1, 5)
    wait_for("save is written at frame 90",
             lambda: log_count("TEST snapshot-save frame=90 code=0") == 1, 60)
    wait_for("original match reaches frame 91", lambda: log_count("TEST shell-crc 91 ") == 1, 60)
    save = next(out.rglob("shell-qa.sav"), None)
    if save is None:
        sys.exit("FAIL the skirmish did not write the named save")

    stop_game(game)
    commands.write_text("89 shell-crc - - 0\n91 shell-crc - - 0\n")
    shell_opens = log_count("Shell::doPush() completed successfully")
    game = launch_game(log_file)
    wait_for("restarting the game opens the main menu",
             lambda: log_count("Shell::doPush() completed successfully") > shell_opens, 100)
    time.sleep(2)
    wait_for("the restarted game window can be positioned for input", move_game_to_origin)
    click(512, 520)
    wait_for("main menu opens the saved games list", lambda: pixel(145, 130) == 0x233A48)
    before = title_signature(635, 246, 245, 220)
    click(258, 228)
    wait_for("selecting the save fills the details card",
             lambda: title_signature(635, 246, 245, 220) != before)
    capture("selected-save", 122, 110, 780, 548)
    contents = save.read_bytes()
    token_end = 1 + contents[0]
    block_size, = struct.unpack_from("<I", contents, token_end)
    if contents[1:token_end] != b"CHUNK_GameState":
        sys.exit("FAIL save fixture has no metadata block")
    error_before = title_signature(294, 602, 434, 38)
    save.write_bytes(contents[:token_end + 4 + block_size] + b"\x06SG_EOF")
    try:
        click(812, 620)
        wait_for("an incomplete save returns to Load with an error",
                 lambda: pixel(145, 130) == 0x233A48
                 and title_signature(294, 602, 434, 38) != error_before, 60)
    finally:
        save.write_bytes(contents)

    key(0xFF54)
    wait_for("Down selects the save after the load error",
             lambda: title_signature(635, 246, 245, 220) != before)
    click(812, 620)
    wait_for("main menu loads the save and returns to frame 91",
             lambda: log_count("TEST shell-crc 91 ") == 2, 100)
    log = (out / "game.log").read_text(errors="replace")
    resumed = re.findall(r"^TEST shell-crc 91 ([A-F0-9]+)$", log, re.MULTILINE)
    if len(resumed) != 2 or resumed[0] != resumed[1] or log_count("TEST shell-crc 89 ") != 1:
        sys.exit(f"FAIL resumed frame 91 CRC differs from the original match: {resumed}")
    print(f"PASS frame 91 CRC matches the original match ({resumed[0]})", flush=True)


def check_loading():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    click(512, 296)
    click(360, 444)
    click(360, 169 + 25)
    click(170, 465)
    click(170, 541)
    click(833, 689, settle=0)
    for frame in range(12):
        capture(f"loading-{frame}", 0, 0, 1024, 768)
        time.sleep(0.15)
    wait_for("the match starts after the loading screen", lambda: len(faction_history()) == 1, 100)
    print("PASS loading screen shown and match started", flush=True)


def check():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    skirmish_label = skirmish_label_pixels()
    if len(skirmish_label) < 10:
        sys.exit("FAIL initial shell has no visible Skirmish label")
    if "--loading" in sys.argv:
        check_loading()
        return
    start_match(1)
    wait_for("first skirmish is running", lambda: len(faction_history()) == 1, 100)
    wait_for("first skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 1, 60)
    if "--spellbook" in sys.argv:
        check_spellbook()
        return
    if "--hud-actions" in sys.argv:
        check_hud_actions()
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


def options_visible():
    return pixel(132, 116) == 0x15232A


def preferences():
    files = list((out / "data").rglob("Options.ini"))
    if not files:
        return {}
    return dict(line.strip().split(" = ", 1) for line in files[0].read_text().splitlines()
                if " = " in line.strip())


def custom_visible():
    return pixel(372, 508) == 0x1D2E36


def move_game_to_origin():
    window = c.c_ulong()
    revert = c.c_int()
    get_focus(display, c.byref(window), c.byref(revert))
    if window.value in (0, 1, root):
        return False
    move_window(display, window.value, 0, 0)
    sync(display, 0)
    return True


def check_options(restarted=False):
    global custom_value
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    wait_for("the game window can be positioned for input", move_game_to_origin)
    if restarted:
        key(0xFF52)
        wait_for("Up from no focus selects Load",
                 lambda: 0xC8AE77 in title_signature(412, 498, 200, 44))
        key(0xFF52)
        wait_for("the Settings entry receives keyboard focus",
                 lambda: 0xC8AE77 in title_signature(412, 442, 200, 48))
        key(0xFF0D)
        wait_for("Settings opens from the main menu by keyboard", options_visible, 8)
    else:
        click(512, 466)
        wait_for("Settings opens from the main menu", options_visible, 8)
    expected = {"Gamma": "75", "MusicVolume": "25", "AmbientVolume": "40", "MovieVolume": "60",
                "ShowAllHealthBars": "1", "ScrollFactor": "75", "RetailStaticGameLOD": "custom",
                "RetailCustomGraphicsPreset": "ultrahigh", "RetailModelDetail": "3"}
    if restarted:
        wait_for("accepted options are present after restart",
                 lambda: all(preferences().get(k, "").lower() == v for k, v in expected.items()))
        click(471, 411)
        wait_for("Custom settings opens after restart", custom_visible)
        wait_for("the selected detail displays its accepted value",
                 lambda: title_signature(519, 298, 70, 41) == custom_value)
        key(0xFF1B)
        wait_for("Escape returns from Custom settings to Video", lambda: options_visible() and not custom_visible())
        for tab_y, name, value in ((219, "video", 75), (273, "audio", 25),
                                   (327, "gameplay", None), (381, "controls", 75)):
            click(237, tab_y)
            x = 606 if value is None else 610 + round(200 * value / 100)
            wait_for(f"{name} control displays the accepted value",
                     lambda: pixel(x, 261) == 0xC8AE77)
            if name == "audio":
                wait_for("ambient and movie controls display their accepted values",
                         lambda: pixel(690, 417) == 0xC8AE77 and pixel(730, 469) == 0xC8AE77)
        click(237, 219)
        initial_mode = title_signature(646, 293, 148, 35)
        click(832, 312)
        wait_for("the resolution draft selects another supported mode",
                 lambda: title_signature(646, 293, 148, 35) != initial_mode)
        click(809, 621)
        wait_for("Accept applies the resolution",
                 lambda: preferences().get("Resolution") not in (None, "1024 768") and log_count("UI: display is now ") > 0)
        width, height = map(int, preferences()["Resolution"].split())
        scale = min(width / 1024, height / 768)
        x = (width - int(1024 * scale)) // 2
        y = (height - int(768 * scale)) // 2
        wait_for("the resized game window can be positioned for input", move_game_to_origin)
        click(round(width / 2), round(height * 466 / 768))
        wait_for("Settings reopens at the new resolution",
                 lambda: pixel(x + int(132 * scale), y + int(116 * scale)) == 0x15232A)
        return
    original = preferences()
    initial_brightness = title_signature(604, 250, 213, 24)
    click(760, 260)
    wait_for("the brightness slider moves", lambda: title_signature(604, 250, 213, 24) != initial_brightness)
    click(521, 621)
    wait_for("Cancel returns to the main menu", lambda: not options_visible())
    if preferences() != original:
        sys.exit("FAIL Cancel changed options on disk")
    click(512, 466)
    wait_for("Settings can be reopened", options_visible)
    wait_for("Cancel restores the brightness slider", lambda: title_signature(604, 250, 213, 24) == initial_brightness)
    initial_quality = title_signature(646, 343, 148, 35)
    click(832, 361)
    wait_for("the graphics preset changes", lambda: title_signature(646, 343, 148, 35) != initial_quality)
    click(240, 621)
    wait_for("Reset restores the graphics preset", lambda: title_signature(646, 343, 148, 35) == initial_quality)
    click(471, 411)
    wait_for("Custom settings opens from Video", custom_visible)
    click(809, 255)
    initial_model = title_signature(519, 298, 70, 41)
    click(510, 318)
    wait_for("a custom model detail changes", lambda: title_signature(519, 298, 70, 41) != initial_model)
    key(0xFF1B)
    wait_for("Cancel closes Custom settings", lambda: options_visible() and not custom_visible())
    click(471, 411)
    wait_for("Custom settings can be reopened", custom_visible)
    click(809, 255)
    wait_for("Cancel discards the custom draft", lambda: title_signature(519, 298, 70, 41) == initial_model)
    click(510, 318)
    custom_value = title_signature(519, 298, 70, 41)
    capture("options-custom", 132, 104, 760, 560)
    click(809, 621)
    wait_for("Done keeps the main settings open", lambda: options_visible() and not custom_visible())
    if preferences() != original:
        sys.exit("FAIL Custom settings modified preferences before Accept")
    for tab_y, name in ((219, "video"), (273, "audio"), (327, "gameplay"), (381, "controls")):
        click(237, tab_y)
        wait_for(f"{name} page is selected", lambda: pixel(137, tab_y) == 0xC8AE77)
        capture("options-" + name, 132, 104, 760, 560)
        click(660 if name == "audio" else 760, 260)
        if name == "audio":
            click(690, 416)
            click(730, 468)
    click(809, 621)
    wait_for("Accept persists changes from all pages",
             lambda: all(preferences().get(k, "").lower() == v for k, v in expected.items()))
    wait_for("Accept returns to the main menu", lambda: not options_visible())
    start_match(1)
    wait_for("a skirmish starts", lambda: len(faction_history()) == 1, 100)
    wait_for("the skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 1, 60)
    key(0xFF1B)
    wait_for("the pause menu opens", menu_visible)
    progress = log_count("TEST powerstate ")
    click(512, 489)
    wait_for("Settings opens from the pause menu", options_visible)
    click(471, 411)
    if custom_visible():
        sys.exit("FAIL Custom settings opens during a match")
    capture("options-pause", 132, 104, 760, 560)
    click(237, 381)
    click(660, 260)
    saved = next((out / "data").rglob("Options.ini"))
    previous_contents = saved.read_bytes()
    permissions = saved.parent.stat().st_mode
    saved.parent.chmod(0o555)
    try:
        click(809, 621)
        wait_for("a rejected write keeps Settings open",
                 lambda: 0xC8AE77 in title_signature(370, 535, 493, 40))
        if saved.read_bytes() != previous_contents:
            sys.exit("FAIL a rejected write changed Options.ini")
    finally:
        saved.parent.chmod(permissions)
    key(0xFF1B)
    wait_for("Escape returns to the pause menu", lambda: not options_visible() and menu_visible())
    if preferences().get("ScrollFactor") != "75":
        sys.exit("FAIL cancelling from Pause changed scroll speed")
    if log_count("TEST powerstate ") != progress:
        sys.exit("FAIL simulation advances while Settings is open")
    click(512, 391)
    wait_for("Resume closes Pause", lambda: not menu_visible())
    wait_for("Resume advances the same skirmish",
             lambda: len(faction_history()) == 1 and log_count("TEST powerstate ") > progress, 45)


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
if "--shell-load" in sys.argv:
    commands.write_text("30 shell-setup - - 0\n89 shell-crc - - 0\n90 snapshot-save - - 0 shell-qa.sav\n91 shell-crc - - 0\n")
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
def launch_game(log):
    return subprocess.Popen(
        [str(build / "GeneralsMD/GeneralsXZH"), "-win", "-xres", "1024", "-yres", "768",
         "-noaudio", "-nomusic", "-novideo", "-noshellmap",
         "-mod", "/game-install/aotr", "-bfme2", "/game-install/bfme2"],
        cwd="/game-install/rotwk", env=env, stdout=log, stderr=subprocess.STDOUT,
    )


def stop_game(process):
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run(check_session):
    global game, log_file
    with (out / "game.log").open("wb") as log_file:
        game = launch_game(log_file)
        try:
            check_session()
        finally:
            stop_game(game)


if "--shell-load" in sys.argv:
    run(check_shell_load)
elif "--options" in sys.argv:
    run(check_options)
    run(lambda: check_options(restarted=True))
else:
    run(check)
