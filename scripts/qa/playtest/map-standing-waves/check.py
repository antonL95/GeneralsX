import re


render_env = {
    "GENERALSX_TEST_STANDING_WAVE_DRAW": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    draws = re.findall(r"^TEST map-standing-wave-draw frame=(\d+) quads=(\d+)$", log, re.MULTILINE)
    if not any(int(quads) > 0 for _, quads in draws):
        raise RuntimeError("The Andrast shoreline submitted no authored wave quads")
