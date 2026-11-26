import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "MordorMountainTroll", "PLAYTEST_FIXED_UPDATE_FPS": "30"}


def check(log_text):
    pattern = re.compile(
        r"^TEST draw frame (\d+) id \d+ MordorMountainTroll module ModuleTag_01 model \S+ clip (\S+) .* flags (\S*) subobjects",
        re.MULTILINE)
    rows = [(int(frame), clip.lower(), set(flags.split(","))) for frame, clip, flags in pattern.findall(log_text)]
    slam = sorted({frame for frame, clip, flags in rows if "PREPARING" in flags and clip.endswith("_grbc")})
    if len(slam) < 20:
        raise ValueError(f"Tree slam drawn for only {len(slam)} frames")
    club = sorted({frame for frame, clip, flags in rows if "PACKING" in flags and clip.endswith("_grbd")})
    if not club or club[0] < slam[-1]:
        raise ValueError("Club lift did not follow the slam")
    if any("PREPARING" in flags for frame, _, flags in rows if frame >= club[0]):
        raise ValueError("PREPARING stayed set after the slam")
