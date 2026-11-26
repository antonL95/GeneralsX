import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "AkhorahilFellBeast", "PLAYTEST_FIXED_UPDATE_FPS": "30"}


def check(log_text):
    pattern = re.compile(
        r"^TEST draw frame (\d+) id \d+ AkhorahilFellBeast module ModuleTag_01 model \S+ clip (\S+) .* flags (\S*) subobjects",
        re.MULTILINE)
    rows = [(int(frame), clip, set(flags.split(","))) for frame, clip, flags in pattern.findall(log_text)]
    screech = [(frame, clip) for frame, clip, flags in rows if "SPECIAL_WEAPON_TWO" in flags]
    if not screech:
        raise ValueError("Screech never set SPECIAL_WEAPON_TWO")
    frames = sorted({frame for frame, _ in screech})
    if len(frames) < 85:
        raise ValueError(f"Screech condition held for only {len(frames)} of 90 frames")
    if any(not clip.endswith("cufellbst_atka") for _, clip in screech):
        raise ValueError(f"Screech drew {sorted({clip for _, clip in screech})}")
    if "SPECIAL_WEAPON_TWO" in rows[-1][2]:
        raise ValueError("Screech condition stayed set")
