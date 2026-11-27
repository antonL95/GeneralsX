import ctypes as c
import os
from pathlib import Path
import re
import shutil
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


def drag(x, y, end_x, end_y):
    motion(display, 0, x, y, 0)
    button(display, 1, 1, 80)
    motion(display, 0, end_x, end_y, 80)
    button(display, 1, 0, 80)
    sync(display, 0)
    time.sleep(1)


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
    setup_title = title_signature(400, 4, 224, 50)
    configure_match(faction_row)
    return setup_title


def configure_match(faction_row):
    click(360, 444)
    click(360, 169 + 25 * faction_row)
    click(170, 465)
    click(170, 541)
    if "--all" in sys.argv or "--restart" in sys.argv:
        capture("setup-before-match", 104, 425, 506, 105)
    click(833, 689)


def menu_visible():
    return pixel(365, 210) == 0x14212A and not subpage_visible()


def subpage_visible():
    return pixel(304, 200) == 0x14212A and pixel(305, 201) == 0x14212A


def confirmation_visible():
    return pixel(365, 318) == 0x14212A and not menu_visible() and not subpage_visible()


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


def patch_metrics(cx, cy, half=12):
    size = half * 2
    image = get_image(display, root, cx - half, cy - half, size, size, 0xFFFFFFFF, 2)
    try:
        pixels = [rgb_tuple(get_pixel(image, x, y)) for y in range(0, size, 2) for x in range(0, size, 2)]
    finally:
        destroy_image(image)
    lumas = [0.299 * r + 0.587 * g + 0.114 * b for r, g, b in pixels]
    spread = max(lumas) - min(lumas)
    chroma = max(max(p) - min(p) for p in pixels)
    return pixels, spread, chroma


def check_dolguldur_buttons():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully") >= 1, 100)
    time.sleep(2)
    start_match(10)
    wait_for("Dol Guldur skirmish is running", lambda: faction_history() == ["FactionGuldur"], 100)
    wait_for("GuldurPorter build column is selected", lambda: log_count("TEST hud 50 ") >= 1, 100)
    wait_for("Dol Guldur skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 1, 60)
    porter_slots = hud_slots_at(50)
    build_slots = [s for s in porter_slots if "ConstructGuldur" in s[1]]
    if len(build_slots) < 4:
        sys.exit(f"FAIL GuldurPorter build column missing construct buttons: {porter_slots}")
    capture("dolguldur-porter-hud", 0, 440, 460, 328)
    porter_slot1, spread1, _ = patch_metrics(288, 578, 8)
    porter_slot2, spread2, _ = patch_metrics(335, 596, 8)
    print(f"INFO GuldurPorter palantir slot1_spread={spread1:.1f} slot2_spread={spread2:.1f}", flush=True)
    if spread2 < 40.0:
        sys.exit(f"FAIL GuldurPorter slot 2 (GuldurShade) is blank/flat (luma_spread={spread2:.1f})")
    arena = title_signature(380, 110, 264, 72)
    click(128, 553, settle=0)
    wait_for("Dol Guldur spellbook opens",
             lambda: sum(abs((before & 0xFF) - (after & 0xFF)) > 24
                         for before, after in zip(arena, title_signature(380, 110, 264, 72)))
                         > len(arena) // 4, interval=0.02)
    time.sleep(0.8)
    capture("dolguldur-spellbook", 220, 60, 584, 660)
    spell_centers = [(386, 246), (512, 246), (638, 246)]
    patches = []
    for idx, (cx, cy) in enumerate(spell_centers, 1):
        pixels, spread, chroma = patch_metrics(cx, cy, 14)
        print(f"INFO spellbook slot {idx} ({cx},{cy}): luma_spread={spread:.1f} max_chroma={chroma}", flush=True)
        if spread < 40.0:
            sys.exit(f"FAIL Dol Guldur spellbook slot {idx} is blank/flat (luma_spread={spread:.1f})")
        patches.append(pixels)
    for i in range(len(patches)):
        for j in range(i + 1, len(patches)):
            mean_diff = sum(
                abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
                for a, b in zip(patches[i], patches[j])
            ) / (3 * len(patches[i]))
            print(f"INFO spellbook slots {i + 1} vs {j + 1}: mean_diff={mean_diff:.1f}", flush=True)
            if mean_diff < 12.0:
                sys.exit(f"FAIL Dol Guldur spellbook slots {i + 1} and {j + 1} look identical (mean_diff={mean_diff:.1f})")
    click(386, 246)
    click(568, 684)
    wait_for("Dol Guldur tier 1 spell purchased", lambda: power_points() and power_points()[-1] <= 10, 60)
    key(0xFF1B)
    wait_for("Dol Guldur spellbook closes", lambda: not menu_visible(), 10)
    time.sleep(0.5)
    capture("dolguldur-left-column", 0, 100, 120, 520)
    print(f"PASS Dol Guldur spellbook and builder buttons rendered ({len(build_slots)} build buttons)", flush=True)


def check_shell_load():
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

    commands.write_text("89 shell-crc - - 0\n91 shell-crc - - 0\n")
    shell_opens = log_count("Shell::doPush() completed successfully")
    restart_game(log_mode="ab")
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
    setup_title = start_match(1)
    wait_for("first skirmish is running", lambda: len(faction_history()) == 1, 100)
    wait_for("first skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 1, 60)
    if "--spellbook" in sys.argv:
        check_spellbook()
        return
    if "--hud-actions" in sys.argv:
        check_hud_actions()
        return
    all_pages = "--all" in sys.argv
    if all_pages or "--save-load" in sys.argv:
        check_save_load()
        if not all_pages:
            return
    if all_pages or "--restart" in sys.argv:
        check_restart()
        if not all_pages:
            return
    if all_pages or "--settings" in sys.argv:
        check_settings()
        if not all_pages:
            return
        key(0xFF1B)
        wait_for("Resume closes Pause after settings", lambda: not menu_visible())
    matches = faction_history()
    first_frame_60_count = log_count("TEST powerstate 60 Player_1")
    key(0xFF1B)
    wait_for("Escape opens the in-game menu", menu_visible)
    capture("pause-menu", 360, 205, 304, 332)
    progress = log_count("TEST powerstate ")
    time.sleep(3)
    if log_count("TEST powerstate ") != progress:
        sys.exit("FAIL simulation advances while the match menu is open")
    print("PASS the match is paused", flush=True)
    click(512, 504)
    wait_for("Resume dismisses the menu", lambda: not menu_visible())
    wait_for("the same match advances again",
             lambda: faction_history() == matches and log_count("TEST powerstate ") > progress, 45)
    capture("hud-menu-button", 54, 529, 150, 71)
    drag(480, 430, 85, 559)
    if menu_visible():
        sys.exit("FAIL releasing a world drag over the HUD button opens Pause")
    click(85, 559)
    wait_for("HUD menu button opens the same pause menu", menu_visible, 12)
    click(512, 453)
    wait_for("Leave opens the confirmation", confirmation_visible)
    capture("leave-confirmation", 360, 312, 304, 170)
    key(0xFF1B)
    wait_for("Escape cancels the leave confirmation", menu_visible)
    click(512, 453)
    wait_for("Leave opens the confirmation again", confirmation_visible)
    click(512, 453)
    wait_for("repeating the Leave click returns to the pause menu", menu_visible)
    click(512, 453)
    wait_for("Leave reopens the confirmation", confirmation_visible)
    click(512, 391)
    wait_for("Leave opens the score screen",
             lambda: not confirmation_visible() and pixel(400, 420) == 0
             and title_signature(400, 4, 224, 50) != setup_title
             and len(skirmish_label & skirmish_label_pixels()) < len(skirmish_label) * 0.8)
    capture("score-screen", 112, 140, 800, 430)
    click(934, 730)
    wait_for("Continue returns to the skirmish lobby",
             lambda: title_signature(400, 4, 224, 50) == setup_title)
    capture("skirmish-lobby", 115, 100, 800, 500)
    configure_match(2)
    wait_for("a second skirmish runs with a different faction",
             lambda: len(faction_history()) == len(matches) + 1 and faction_history()[-1] != matches[-1], 100)
    wait_for("second skirmish reaches frame 60",
             lambda: log_count("TEST powerstate 60 Player_1") > first_frame_60_count, 60)
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
    click(512, 249)
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
    click(512, 504)
    wait_for("Resume closes Pause", lambda: not menu_visible())
    wait_for("Resume advances the same skirmish",
             lambda: len(faction_history()) == 1 and log_count("TEST powerstate ") > progress, 45)


def crc_history():
    log = (out / "game.log").read_text(errors="replace")
    start = log.find("TEST player 30 Player_1 template=")
    if start < 0:
        return []
    return [(int(frame), crc) for frame, crc in re.findall(
        r"TEST xp-crc frame=(\d+) crc=([0-9A-F]+)", log[start:])]


def check_save_load():
    key(0xFF1B)
    wait_for("Pause opens before saving", menu_visible, 15)
    click(512, 300)
    wait_for("Save opens the save page", subpage_visible, 10)
    for char in "slotfour":
        key(ord(char))
    wait_for("the typed save name is visible", lambda: sum(
        min(color.to_bytes(4, "big")[1:]) > 150
        for color in title_signature(336, 486, 120, 25)) >= 10)
    capture("save-menu", 230, 195, 564, 378)
    key(0xFF0D)
    wait_for("Save creates a file and returns to Pause",
             lambda: menu_visible() and list((out / "data").rglob("*.sav")), 15)
    save = next((out / "data").rglob("*.sav"))
    contents = save.read_bytes()
    if not any("slotfour".encode(encoding) in contents for encoding in ("utf-16-le", "utf-32-le")):
        sys.exit("FAIL save file has no entered slot name")
    saved_at = len(crc_history())
    key(0xFF1B)
    wait_for("the uninterrupted match advances", lambda: len(crc_history()) >= saved_at + 35, 30)
    motion(display, 0, 690, 581, 0)
    sync(display, 0)
    time.sleep(0.5)
    key(0xFF1B)
    wait_for("Pause reopens before loading", menu_visible, 15)
    for name, y in (("Leave", 453), ("Restart", 402), ("Load", 351)):
        key(0xFF52)
        wait_for(f"Up selects {name}", lambda: pixel(386, y) == 0x365665, 10)
    key(0xFF0D)
    wait_for("Load opens the save list", subpage_visible, 10)
    capture("load-menu", 230, 195, 564, 378)
    feedback = title_signature(321, 487, 382, 27)
    save.write_bytes(contents[:-32])
    key(0xFF0D)
    wait_for("a truncated save reports an error without closing Load",
             lambda: subpage_visible() and title_signature(321, 487, 382, 27) != feedback)
    save.write_bytes(contents)
    key(0xFF0D)
    def loaded():
        history = crc_history()
        return any(b[0] <= a[0] for a, b in zip(history, history[1:]))

    wait_for("loading rewinds the same skirmish", loaded, 120)
    history = crc_history()
    split = next(i for i in range(1, len(history)) if history[i][0] <= history[i - 1][0])
    expected = dict(history[:split])
    first_frame = history[split][0]
    wait_for("the loaded match advances", lambda: len(crc_history()) >= split + 30, 30)
    compared = 0
    different = 0
    for frame, crc in crc_history()[split:]:
        if frame in expected:
            compared += 1
            if crc != expected[frame]:
                print(f"CRC mismatch at {frame}: {expected[frame]} != {crc}", flush=True)
                different += 1
    print(f"Compared {compared} frames from {first_frame}. Differences {different}", flush=True)
    if compared < 30 or different:
        sys.exit("FAIL menu save and resume CRCs")


def match_factions():
    log = (out / "game.log").read_text(errors="replace")
    return re.findall(r"TEST player 30 (Player_\d+) template=(\S+)", log)


def check_restart():
    initial = match_factions()
    if len(initial) < 2:
        sys.exit("FAIL restart needs both skirmish factions")
    key(0xFF1B)
    wait_for("Pause opens before restart", menu_visible)
    click(512, 402)
    wait_for("Restart opens a confirmation", confirmation_visible, 8)
    capture("restart-confirmation", 360, 312, 304, 170)
    click(580, 447)
    wait_for("No returns from Restart to Pause", menu_visible)
    click(512, 402)
    wait_for("Restart reopens a confirmation", confirmation_visible)
    click(437, 447)
    wait_for("restarted skirmish reaches frame 30", lambda: len(match_factions()) == len(initial) * 2, 120)
    if match_factions() != initial + initial:
        sys.exit(f"FAIL restart changed the factions: {match_factions()}")
    print("PASS restart rewound to frame 30 with factions " + ", ".join(f"{p}={f}" for p, f in initial), flush=True)
    wait_for("restarted skirmish reaches frame 60", lambda: log_count("TEST powerstate 60 Player_1") >= 2, 60)


def check_settings():
    key(0xFF1B)
    wait_for("Pause opens before settings", menu_visible)
    click(512, 249)
    wait_for("Settings opens", options_visible)
    capture("settings", 132, 104, 760, 560)
    click(237, 381)
    click(660, 260)
    key(0xFF1B)
    wait_for("Settings Cancel returns to Pause", lambda: not options_visible() and menu_visible())
    options = next((out / "data").rglob("Options.ini"), None)
    if options and "ScrollFactor" in options.read_text():
        sys.exit("FAIL cancelling Settings saved ScrollFactor")
    click(512, 249)
    wait_for("Settings opens again", options_visible)
    click(237, 381)
    click(660, 260)
    click(809, 621)
    wait_for("Settings Accept returns to Pause", lambda: not options_visible() and menu_visible())
    if not preferences().get("ScrollFactor"):
        sys.exit("FAIL accepting Settings did not save ScrollFactor")
    print("PASS Settings persists the edited ScrollFactor", flush=True)


def check_create_a_hero():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    before = title_signature(55, 152, 365, 500)
    click(512, 576)
    wait_for("Create a Hero opens from the main menu",
             lambda: title_signature(55, 152, 365, 500) != before, 5)
    initial_roster = title_signature(59, 185, 355, 422)
    capture("hero-manager", 52, 150, 910, 525)
    click(320, 641)
    capture("hero-class", 595, 105, 405, 593)
    click(873, 735)
    click(801, 237)
    for char in "slot three":
        key(ord(char))
    click(942, 304)
    capture("hero-appearance", 577, 99, 427, 604)
    click(873, 735)
    capture("hero-attributes", 577, 99, 427, 604)
    click(873, 735)
    click(128, 193)
    capture("hero-powers", 70, 142, 900, 545)
    click(873, 735)
    save_dir = out / "data/GeneralsX/GeneralsZH/Save"
    wait_for("the hero is saved to the profile Save directory",
             lambda: len(list(save_dir.glob("MyHero_*.cah"))) == 1)
    record = next(save_dir.glob("MyHero_*.cah")).read_bytes()
    if not record.startswith(b"ALAE2STR") or "slot three".encode("utf-16le") not in record or b"Command_CreateAHero_" not in record:
        sys.exit("FAIL the saved hero record lacks its name or selected power")
    click(152, 208)
    saved_row = title_signature(59, 185, 355, 422)
    if saved_row == initial_roster:
        sys.exit("FAIL the new hero is missing from the roster")
    restart_game()
    wait_for("main menu opens after restart", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    click(512, 576)
    wait_for("the same saved roster is displayed after restart",
             lambda: title_signature(59, 185, 355, 422) == saved_row)
    capture("hero-list-after-restart", 53, 151, 910, 525)
    click(145, 735)
    click(512, 296)
    click(858, 444)
    capture("hero-skirmish-choice", 765, 403, 157, 145)
    click(858, 517)
    click(833, 689)
    click(833, 689)
    wait_for("fortress recruitment uses the hero slot",
             lambda: log_count("slot 21 Command_CreateAHeroReviveSlot type 53 enabled"), 75)
    wait_for("the fortress queues the configured hero at the authored price",
             lambda: re.search(r"TEST hero 50 Player_1 1 CreateAHero .*queued=1 .*cost=2125 frames=2100",
                               (out / "game.log").read_text(errors="replace")))
    wait_for("the queued recruitment saves", lambda: (save_dir / "hero-queued.sav").is_file() and
             log_count("TEST snapshot-save frame=55 code=0"))
    (out / "recruitment.log").write_text((out / "game.log").read_text(errors="replace"))
    restart_game("-headless", "-noFPSLimit", "-loadsave", "hero-queued.sav")
    wait_for("Berethor is recruited with his authored first power",
             lambda: re.search(r"TEST pkg18 hero frame=2260 .*name=Berethor .*hasCheck=1 .*"
                               r"firstPower=Command_CreateAHero_HotW_SummonAllies_Level1",
                               (out / "game.log").read_text(errors="replace")), 100)
    wait_for("the rank two power stays locked at rank one",
             lambda: re.search(r"TEST pkg18 hero frame=2261 .*hasCheck=0 "
                               r"checkName=Upgrade_CreateAHeroCreateAHeroTrainAllies_Level_1 .*rank=1 ",
                               (out / "game.log").read_text(errors="replace")))
    wait_for("the recruited hero match saves",
             lambda: (save_dir / "hero-skirmish.sav").is_file() and log_count("TEST powercrc 2266 "))
    original = (out / "game.log").read_text(errors="replace")
    expected = re.search(r"TEST powercrc 2266 ([0-9A-F]{8})", original).group(1)
    (out / "recruited.log").write_text(original)
    restart_game("-loadsave", "hero-skirmish.sav")
    wait_for("resumed match preserves the game CRC",
             lambda: log_count(f"TEST powercrc 2266 {expected}"), 100)
    wait_for("Berethor and his first power survive resume",
             lambda: re.search(r"TEST pkg18 hero frame=2270 .*name=Berethor .*hasCheck=1 .*"
                               r"firstPower=Command_CreateAHero_HotW_SummonAllies_Level1",
                               (out / "game.log").read_text(errors="replace")))
    capture("hero-recruited", 0, 480, 1024, 288)
    wait_for("the selected rank two power unlocks when Berethor levels up",
             lambda: re.search(r"TEST pkg18 hero frame=2285 .*hasCheck=1 "
                               r"checkName=Upgrade_CreateAHeroCreateAHeroTrainAllies_Level_1 ",
                               (out / "game.log").read_text(errors="replace")))


def rgb_tuple(value):
    return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF


def near_color(rgb, target, tol=16):
    return all(abs(a - b) <= tol for a, b in zip(rgb, target))


def measure_palantir_bar(x_offset=0):
    image = get_image(display, root, 240 + x_offset, 700, 100, 16, 0xFFFFFFFF, 2)
    try:
        border = (199, 164, 94)
        fill = (232, 183, 51)
        hits = []
        for dy in range(16):
            for dx in range(100):
                rgb = rgb_tuple(get_pixel(image, dx, dy))
                if near_color(rgb, border, 18) or near_color(rgb, fill, 18):
                    hits.append((240 + x_offset + dx, 700 + dy))
        return hits
    finally:
        destroy_image(image)


def assert_bar_layout(label):
    hits = measure_palantir_bar()
    if len(hits) < 40:
        capture("failed-bar-" + label, 200, 580, 185, 185)
        sys.exit(f"FAIL {label}: expected palantir bar pixels, found {len(hits)}")
    xs = [p[0] for p in hits]
    ys = [p[1] for p in hits]
    min_x, max_x, min_y, max_y = min(xs), max(xs), min(ys), max(ys)
    overflow = [p for p in hits if p[0] >= 311]
    if overflow or min_x < 254 or max_x > 310 or min_y < 703 or max_y > 711:
        capture("failed-bar-" + label, 200, 580, 185, 185)
        sys.exit(f"FAIL {label}: bar bounds x={min_x}..{max_x} y={min_y}..{max_y} overflow={len(overflow)}")
    for px, py in ((min_x, min_y), (max_x, min_y), (min_x, max_y), (max_x, max_y)):
        if (px - 288) ** 2 + (py - 662) ** 2 > 76 ** 2:
            sys.exit(f"FAIL {label}: bar corner ({px},{py}) outside portrait circle")
    if (max_x - 329) ** 2 + (max_y - 726) ** 2 <= 21 ** 2:
        sys.exit(f"FAIL {label}: bar overlaps RingButton5 socket")
    if max_y >= 717:
        sys.exit(f"FAIL {label}: bar overlaps RingButton6 socket")
    print(f"PASS {label}: bar x={min_x}..{max_x} y={min_y}..{max_y} clear of buttons", flush=True)


def check_palantir_bars():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    start_match(0)
    wait_for("Aragorn is selected for level bar check", lambda: log_count("TEST hud 40 ") >= 1, 100)
    wait_for("level bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("palantir-level-bar", 200, 580, 185, 185)
    assert_bar_layout("level-bar")
    wait_for("summoned horde is selected for timer bar check", lambda: log_count("TEST hud 90 ") >= 1, 100)
    wait_for("timer bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("palantir-timer-bar", 200, 580, 185, 185)
    assert_bar_layout("timer-bar")


def hero_roster_samples():
    image = get_image(display, root, 474, 699, 56, 56, 0xFFFFFFFF, 2)
    try:
        return [
            rgb_tuple(get_pixel(image, dx, dy))
            for dx, dy in ((8, 22), (8, 28), (10, 18), (46, 22), (46, 28), (44, 18))
        ]
    finally:
        destroy_image(image)


def check_hero_roster():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    start_match(0)
    wait_for("Haldir is spawned on the hero roster", lambda: log_count("TEST hud 45 ") >= 1, 100)
    wait_for("hero roster backing has no opaque black disc",
             lambda: any(r > 10 or g > 10 or b > 10 for r, g, b in hero_roster_samples()), 15)
    capture("hero-roster", 360, 680, 220, 88)


def hud_slots_at(frame):
    log = (out / "game.log").read_text(errors="replace")
    return re.findall(rf"^TEST hud {frame} slot=(\d+) (\S+) type=(\d+) (\S+)", log, re.MULTILINE)


def check_multi_select():
    wait_for("main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    start_match(0)

    wait_for("all-heroes multi-select hud logged", lambda: log_count("TEST hud 50 ") >= 1, 100)
    wait_for("all-heroes bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("multi-all-heroes", 200, 580, 185, 185)
    slots_heroes = hud_slots_at(50)
    ring_heroes = [s for s in slots_heroes if int(s[0]) < 6]
    if len(ring_heroes) < 2:
        sys.exit(f"FAIL all-heroes multi-select showed too few ring buttons: {slots_heroes}")
    assert_bar_layout("multi-all-heroes")
    print(f"PASS all-heroes multi-select buttons={[s[1] for s in ring_heroes]}", flush=True)

    wait_for("mixed hero+battalion multi-select hud logged", lambda: log_count("TEST hud 90 ") >= 1, 100)
    wait_for("mixed hero+battalion bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("multi-mixed-hero", 200, 580, 185, 185)
    slots_mixed_hero = hud_slots_at(90)
    ring_mixed_hero = [s for s in slots_mixed_hero if int(s[0]) < 6]
    if [s[1] for s in ring_mixed_hero] != [s[1] for s in ring_heroes]:
        sys.exit(f"FAIL mixed hero+battalion buttons {ring_mixed_hero} != hero buttons {ring_heroes}")
    assert_bar_layout("multi-mixed-hero")
    print(f"PASS mixed hero+battalion multi-select shows hero buttons and level", flush=True)

    wait_for("same-type battalions multi-select hud logged", lambda: log_count("TEST hud 130 ") >= 1, 100)
    wait_for("same-type battalions bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("multi-same-battalions", 200, 580, 185, 185)
    slots_same = hud_slots_at(130)
    ring_same = [s for s in slots_same if int(s[0]) < 6]
    if len(ring_same) < 3:
        sys.exit(f"FAIL same-type battalions multi-select showed too few ring buttons: {slots_same}")
    assert_bar_layout("multi-same-battalions")
    print(f"PASS same-type battalions multi-select buttons={[s[1] for s in ring_same]}", flush=True)

    wait_for("mixed non-hero battalions multi-select hud logged", lambda: log_count("TEST hud 170 ") >= 1, 100)
    wait_for("mixed battalions bar frame is presented", lambda: len(measure_palantir_bar()) >= 40, 15)
    capture("multi-mixed-battalions", 200, 580, 185, 185)
    slots_mixed = hud_slots_at(170)
    ring_mixed = [s for s in slots_mixed if int(s[0]) < 6]
    if not any("Stance" in s[1] for s in ring_mixed):
        sys.exit(f"FAIL mixed battalions multi-select missing common stance button: {slots_mixed}")
    assert_bar_layout("multi-mixed-battalions")
    print(f"PASS mixed battalions multi-select buttons={[s[1] for s in ring_mixed]}", flush=True)

    wait_for("mixed battalion+porter multi-select hud logged", lambda: log_count("TEST hud 210 ") >= 1, 100)
    wait_for("mixed battalion+porter hides level bar", lambda: len(measure_palantir_bar()) < 20, 15)


def count_red_pixels(x, y, width, height):
    image = get_image(display, root, x, y, width, height, 0xFFFFFFFF, 2)
    try:
        hits = 0
        for dy in range(height):
            for dx in range(width):
                r, g, b = rgb_tuple(get_pixel(image, dx, dy))
                if r > g + 60 and r > b + 60:
                    hits += 1
        return hits
    finally:
        destroy_image(image)


def prepare_ui_vfs_override():
    override_mod = out / "override-mod"
    override_mod.mkdir()
    aotr_root = Path("/game-install/aotr")
    for entry in aotr_root.iterdir():
        if entry.name.lower() == "data":
            continue
        (override_mod / entry.name).symlink_to(entry)
    data_dir = override_mod / "data"
    data_dir.mkdir()
    for entry in (aotr_root / "data").iterdir():
        if entry.name.lower() == "ini":
            continue
        (data_dir / entry.name).symlink_to(entry)
    ini_dir = data_dir / "ini"
    ini_dir.mkdir()
    for entry in (aotr_root / "data" / "ini").iterdir():
        (ini_dir / entry.name).symlink_to(entry)
    (ini_dir / "UiFrames.ini").write_text(
        "UiFrame MenuButtonMed\n"
        "\tColor = R:255 G:32 B:32 A:255\n"
        "End\n"
    )
    wnd_source = Path(__file__).resolve().parents[2] / "Core/GameEngine/Source/GameClient/RetailHud.wnd"
    wnd_text = wnd_source.read_text()
    target = (
        '\t\tNAME = "ControlBar.wnd:Palantir";\n'
        '\t\tSTATUS = ENABLED;\n'
        '\t\tSTYLE = USER;\n'
        '\t\tANCHOR = LEFT BOTTOM;\n'
        '\t\tOFFSET = 0 0;\n'
    )
    replacement = target.replace("OFFSET = 0 0;", "OFFSET = 120 0;")
    if wnd_text.count(target) != 1:
        sys.exit("FAIL could not patch Palantir offset in RetailHud.wnd fixture")
    wnd_dir = data_dir / "window"
    wnd_dir.mkdir()
    (wnd_dir / "RetailHud.wnd").write_text(wnd_text.replace(target, replacement, 1))
    return override_mod


def check_ui_vfs():
    wait_for("baseline main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    click(512, 296)
    capture("ui-vfs-baseline-menu", 735, 664, 190, 52)
    baseline_red = count_red_pixels(739, 668, 181, 44)
    configure_match(0)
    wait_for("baseline Aragorn HUD logged", lambda: log_count("TEST hud 40 ") >= 1, 100)
    wait_for("baseline Palantir bar is at default x", lambda: len(measure_palantir_bar(0)) >= 40, 15)
    capture("ui-vfs-baseline-hud", 0, 500, 620, 268)
    baseline_slots = hud_slots_at(40)
    baseline_at_0 = len(measure_palantir_bar(0))
    baseline_at_120 = len(measure_palantir_bar(120))
    if baseline_at_120 >= 20:
        sys.exit(f"FAIL baseline Palantir bar unexpectedly present at +120: {baseline_at_120}")
    print(f"PASS ui-vfs baseline: red={baseline_red} bar_at_0={baseline_at_0} bar_at_120={baseline_at_120}", flush=True)

    override_mod = prepare_ui_vfs_override()
    restart_game(mod_dir=str(override_mod))
    wait_for("override main menu opens", lambda: log_count("Shell::doPush() completed successfully"), 100)
    time.sleep(2)
    click(512, 296)
    capture("ui-vfs-override-menu", 735, 664, 190, 52)
    override_red = count_red_pixels(739, 668, 181, 44)
    if override_red <= baseline_red + 200:
        sys.exit(f"FAIL partial UiFrames.ini override did not tint MenuButtonMed: baseline_red={baseline_red} override_red={override_red}")
    configure_match(0)
    wait_for("override Aragorn HUD logged", lambda: log_count("TEST hud 40 ") >= 1, 100)
    wait_for("override Palantir bar moved +120px", lambda: len(measure_palantir_bar(120)) >= 40, 15)
    capture("ui-vfs-override-hud", 0, 500, 620, 268)
    override_at_0 = len(measure_palantir_bar(0))
    override_at_120 = len(measure_palantir_bar(120))
    if override_at_0 >= 20:
        sys.exit(f"FAIL override Palantir bar still at default x: {override_at_0}")
    override_slots = hud_slots_at(40)
    if override_slots != baseline_slots:
        sys.exit(f"FAIL override HUD slots changed: {override_slots} != {baseline_slots}")
    print(f"PASS ui-vfs override: red={override_red} bar_at_0={override_at_0} bar_at_120={override_at_120}", flush=True)


def roster_at(frame):
    text = (out / "game.log").read_text(errors="replace")
    rows = re.findall(rf"TEST roster {frame} slot=\d+ (\S+) builders=(\d+)", text)
    heroes = sorted(name for name, _ in rows if name != "builder")
    builders = sum(int(count) for name, count in rows if name == "builder")
    return heroes, builders


def hover_world(frame, expected, kind, image=None, sample_delay=120):
    wait_for(f"camera moves for hover {frame}", lambda: log_count(f"TEST frame {frame}: look ") >= 1, 100)
    wait_for(f"screen position {frame + 30}", lambda: log_count(f"TEST frame {frame + 30}: screenpos ") >= 1, 100)
    x, y = map(int, re.search(rf"TEST frame {frame + 30}: screenpos (\d+) (\d+)", (out / "game.log").read_text(errors="replace")).groups())
    motion(display, 0, x - 40, y - 40, 0)
    sync(display, 0)
    time.sleep(0.3)
    motion(display, 0, x, y, 0)
    sync(display, 0)
    dumped = frame + sample_delay
    wait_for(f"tooltip dump {dumped}", lambda: log_count(f"TEST tooltip {dumped} ") >= 1, 100)
    text = (out / "game.log").read_text(errors="replace")
    line = re.search(rf"TEST tooltip {dumped} shown=(\d) kind=(\w+) text=(.*)", text)
    expected_text = (expected,) if isinstance(expected, str) else expected
    if line.group(1) != "1" or line.group(2) != kind or any(text not in line.group(3) for text in expected_text):
        sys.exit(f"FAIL hover {frame}: expected {kind} with {expected!r}, got {line.group(0)}")
    print(f"PASS hover {line.group(0)}", flush=True)
    if image:
        capture(image, 0, 0, 1024, 768)


def check_hover_roster():
    wait_for("roster logged after spawns", lambda: log_count("TEST hud 90 ") + log_count("TEST tooltip 90 ") >= 1, 100)
    before_heroes, before_builders = roster_at(45)
    heroes, builders = roster_at(90)
    if heroes != sorted(before_heroes + ["GondorAragorn", "GondorBoromir"]) or builders != before_builders + 1:
        sys.exit(f"FAIL roster heroes={heroes} builders={builders} before={before_heroes},{before_builders}")
    print(f"PASS roster heroes={heroes} builders={builders}", flush=True)
    hover_world(100, "Builder", "box", "hover-info")
    hover_world(230, "Troll", "label")
    hover_world(360, "Fortress", "box")


def check_hero_roster_filter():
    wait_for("hero roster sampled after fixture spawns",
             lambda: log_count("TEST roster 35 ") >= 1, 150)
    before, _ = roster_at(12)
    after, _ = roster_at(35)
    expected = sorted(before + ["GondorAragorn", "RosterFixtureUnselectable"])
    if after != expected:
        sys.exit(f"FAIL hero roster image filter expected={expected} actual={after}")
    print(f"PASS hero roster image filter entries={after}", flush=True)
    capture("hero-roster-filter", 360, 680, 220, 88)


def check_veteran_hover():
    hover_world(12, ("Soldier of Minas Tirith of VeteranGuard", "Battles Fought: 4"), "box", "veteran-hover",
                sample_delay=90)
    hover_world(110, "Soldier of Minas Tirith", "label", sample_delay=90)


def launch_game(*options, log_mode="wb", mod_dir="/game-install/aotr"):
    speed = ["-fps", "120"] if "--create-a-hero" in sys.argv else []
    with (out / "game.log").open(log_mode) as log:
        return subprocess.Popen(
            ["timeout", "600" if "--hover-roster" in sys.argv else "300", str(build / "GeneralsMD/GeneralsXZH"),
             "-win", "-xres", "1024", "-yres", "768",
             "-noaudio", "-nomusic", "-novideo", "-noshellmap",
             "-mod", mod_dir, "-bfme2", "/game-install/bfme2", *speed, *options],
            cwd="/game-install/rotwk", env=env, stdout=log, stderr=subprocess.STDOUT,
        )


def stop_game(process=None):
    target = game if process is None else process
    target.terminate()
    try:
        target.wait(timeout=10)
    except subprocess.TimeoutExpired:
        target.kill()
        target.wait()


def restart_game(*options, log_mode="wb", mod_dir="/game-install/aotr"):
    global game
    stop_game()
    game = launch_game(*options, log_mode=log_mode, mod_dir=mod_dir)


build = Path(sys.argv[1]).resolve()
out = Path(tempfile.mkdtemp(prefix="pause-qa.", dir=build))
print(f"Run directory {out}", flush=True)
runtime = out / "runtime"
runtime.mkdir(mode=0o700)
commands = out / "commands.txt"
commands.write_text(("15 pkg44-points Player_1 - 0 10\n" if "--spellbook" in sys.argv else "") +
                    "30 player Player_1 - 0\n" +
                    ("".join(f"30 player Player_{slot} - 0\n" for slot in range(2, 9))
                     if "--restart" in sys.argv or "--all" in sys.argv else "") + "".join(
    f"{frame} powerstate Player_1 - 0\n" for frame in range(60, 3600, 30)
) + ("".join(f"{frame} xpcrc - - 0\n" for frame in range(60, 900))
     if "--save-load" in sys.argv or "--all" in sys.argv else ""))
if "--dolguldur-buttons" in sys.argv:
    commands.write_text(
        "15 pkg44-points Player_1 - 0 10\n"
        "30 player Player_1 - 0\n"
        "40 select Player_1 GuldurPorter 0\n"
        "50 hud - - 0\n"
        + "".join(f"{frame} powerstate Player_1 - 0\n" for frame in range(60, 3600, 30))
    )
if "--shell-load" in sys.argv:
    commands.write_text("30 shell-setup - - 0\n89 shell-crc - - 0\n90 snapshot-save - - 0 shell-qa.sav\n91 shell-crc - - 0\n")
if "--palantir-bars" in sys.argv:
    commands.write_text(
        "20 spawn Player_1 GondorAragorn 0\n"
        "30 select Player_1 GondorAragorn 0\n"
        "40 hud - - 0\n"
        "70 spawn Player_1 DwarvenPhalanxHorde_Summoned 0\n"
        "80 select Player_1 DwarvenPhalanxHorde_Summoned 0\n"
        "90 hud - - 0\n"
    )
if "--ui-vfs" in sys.argv:
    commands.write_text(
        "20 spawn Player_1 GondorAragorn 0\n"
        "30 select Player_1 GondorAragorn 0\n"
        "40 hud - - 0\n"
    )
if "--hero-roster" in sys.argv:
    commands.write_text(
        "20 spawn Player_1 ElvenHaldir 0\n"
        "30 select Player_1 ElvenHaldir 0\n"
        "45 hud - - 0\n"
    )
if "--hover-roster" in sys.argv:
    commands.write_text(
        "45 hud - - 0\n"
        "50 spawn Player_1 GondorAragorn 0 220,0\n"
        "51 spawn Player_1 GondorBoromir 0 220,60\n"
        "52 spawn Player_1 MenPorter 0 220,-60\n"
        "53 spawn Player_1 GondorWorker 0 260,-60\n"
        "54 spawn Player_1 DunharrowSleeplessDead_Summoned 0 260,0\n"
        "55 create PlyrCreeps MordorMountainTroll 0 1000,1750,0\n"
        "56 reveal Player_1 MenFortressCitadel 0\n"
        "90 hud - - 0\n"
        "100 look Player_1 MenPorter 0\n"
        "130 screenpos Player_1 MenPorter 0\n"
        "220 hud - - 0\n"
        "230 look PlyrCreeps MordorMountainTroll 0\n"
        "260 screenpos PlyrCreeps MordorMountainTroll 0\n"
        "350 hud - - 0\n"
        "360 look Player_1 MenFortressCitadel 0\n"
        "390 screenpos Player_1 MenFortressCitadel 0\n"
        "480 hud - - 0\n"
    )
if "--multi-select" in sys.argv:
    commands.write_text(
        "20 spawn Player_1 GondorAragorn 0\n"
        "21 spawn Player_1 GondorBoromir 0\n"
        "22 spawn Player_1 GondorFighterHorde 0\n"
        "23 spawn Player_1 GondorFighterHorde 0\n"
        "24 spawn Player_1 GondorArcherHorde 0\n"
        "25 spawn Player_1 MenPorter 0\n"
        "35 select Player_1 GondorAragorn 0\n"
        "36 selectadd Player_1 GondorBoromir 0\n"
        "50 hud - - 0\n"
        "75 select Player_1 GondorFighterHorde 0\n"
        "76 selectadd Player_1 GondorAragorn 0\n"
        "90 hud - - 0\n"
        "115 select Player_1 GondorFighterHorde 0\n"
        "116 selectadd Player_1 GondorFighterHorde 1\n"
        "130 hud - - 0\n"
        "155 select Player_1 GondorFighterHorde 0\n"
        "156 selectadd Player_1 GondorArcherHorde 0\n"
        "170 hud - - 0\n"
        "195 select Player_1 GondorFighterHorde 0\n"
        "196 selectadd Player_1 MenPorter 0\n"
        "210 hud - - 0\n"
    )
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
if any(option in sys.argv for option in (
        "--create-a-hero", "--hover-roster", "--hero-roster-filter", "--veteran-hover")):
    map_dir = out / "data/GeneralsX/GeneralsZH/Maps/aotr training grounds"
    map_dir.mkdir(parents=True)
    source = Path("/game/UserData/Maps/AotR Training Grounds")
    for file in source.iterdir():
        shutil.copy2(file, map_dir / (file.name.lower() if file.suffix.lower() == ".map" else file.name))
if "--hero-roster-filter" in sys.argv:
    with (map_dir / "map.ini").open("a") as map_ini:
        map_ini.write("\nChildObject RosterFixtureMissingArt GondorBoromir\n"
                      "    ButtonImage = RosterFixtureUnmappedButton\n"
                      "End\n\n"
                      "ChildObject RosterFixtureUnselectable GondorBoromir\n"
                      "    KindOf = -SELECTABLE +SUMMONED +IGNORES_SELECT_ALL\n"
                      "End\n\n"
                      "ChildObject RosterFixtureUnmappedEquivalent GondorAragorn\n"
                      "    EquivalentTo = GondorAragorn\n"
                      "    ButtonImage = RosterFixtureUnmappedButton\n"
                      "End\n")
    commands.write_text(
        "12 hud - - 0\n"
        "20 create Player_1 GondorAragorn 0 700,2450,0\n"
        "21 create Player_1 RosterFixtureMissingArt 0 800,2450,0\n"
        "22 create Player_1 RosterFixtureUnselectable 0 900,2450,0\n"
        "23 create Player_1 RosterFixtureUnmappedEquivalent 0 1000,2450,0\n"
        "35 hud - - 0\n"
    )
if "--veteran-hover" in sys.argv:
    help_off = out / "help-off.ini"
    help_off.write_text("GameData\n    UseHelpTextSystem = No\nEnd\n")
    commands.write_text(
        "10 create Player_1 GondorFighterHorde 0 700,2450,0\n"
        "12 look Player_1 GondorFighter 0\n"
        "30 world-hover-veteran Player_1 GondorFighterHorde 0 VeteranGuard\n"
        "42 screenpos Player_1 GondorFighter 0\n"
        "102 hud - - 0\n"
        f"106 combatini - - 0 {help_off}\n"
        "110 look Player_1 GondorFighter 0\n"
        "140 screenpos Player_1 GondorFighter 0\n"
        "200 hud - - 0\n"
    )
if "--create-a-hero" in sys.argv:
    commands.write_text(
        "10 cash Player_1 - 0 10000\n"
        "20 select Player_1 MenFortressCitadel 0\n"
        "30 slot - - 0 21\n"
        "41 hud - - 0\n"
        "42 slot - - 0 21\n"
        "50 heroes Player_1 - 0\n"
        "55 snapshot-save - - 0 hero-queued.sav\n"
        "2260 pkg18-hero-state Player_1 CreateAHero 0 Upgrade_CreateAHeroHotWSummonAllies_Level1\n"
        "2261 pkg18-hero-state Player_1 CreateAHero 0 Upgrade_CreateAHeroCreateAHeroTrainAllies_Level_1\n"
        "2265 snapshot-save - - 0 hero-skirmish.sav\n"
        "2266 powercrc - - 0\n"
        "2268 select Player_1 CreateAHero 0\n"
        "2270 pkg18-hero-state Player_1 CreateAHero 0 Upgrade_CreateAHeroHotWSummonAllies_Level1\n"
        "2284 pkg18-award-xp Player_1 CreateAHero 0 100000\n"
        "2285 pkg18-hero-state Player_1 CreateAHero 0 Upgrade_CreateAHeroCreateAHeroTrainAllies_Level_1\n"
    )
def run(check_session, *options):
    global game
    game = launch_game(*options)
    try:
        check_session()
    finally:
        stop_game(game)


if "--create-a-hero" in sys.argv:
    run(check_create_a_hero)
elif "--palantir-bars" in sys.argv:
    run(check_palantir_bars)
elif "--hero-roster" in sys.argv:
    run(check_hero_roster)
elif "--hover-roster" in sys.argv:
    run(check_hover_roster, "-file", str(map_dir / "aotr training grounds.map"))
elif "--hero-roster-filter" in sys.argv:
    run(check_hero_roster_filter, "-file", str(map_dir / "aotr training grounds.map"))
elif "--veteran-hover" in sys.argv:
    run(check_veteran_hover, "-file", str(map_dir / "aotr training grounds.map"))
elif "--multi-select" in sys.argv:
    run(check_multi_select)
elif "--dolguldur-buttons" in sys.argv:
    run(check_dolguldur_buttons)
elif "--shell-load" in sys.argv:
    run(check_shell_load)
elif "--ui-vfs" in sys.argv:
    run(check_ui_vfs)
elif "--options" in sys.argv:
    run(check_options)
    run(lambda: check_options(restarted=True))
else:
    run(check)
