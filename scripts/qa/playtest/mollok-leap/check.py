import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "MordorMollok", "PLAYTEST_FIXED_UPDATE_FPS": "30"}


def check(log_text):
    pattern = re.compile(
        r"^TEST draw frame (\d+) id \d+ MordorMollok module ModuleTag_01 model \S+ clip (\S+) at ([\d.]+)/([1-9]\d*) .* flags (\S*) subobjects",
        re.MULTILINE)
    rows = [(int(frame), clip, float(pose), int(length), set(flags.split(",")))
            for frame, clip, pose, length, flags in pattern.findall(log_text)]
    expected = (("UNPACKING", "mumollok_lepa"), ("THROWN_PROJECTILE", "mumollok_lepb"), ("ABOUT_TO_HIT", "mumollok_lepc"))
    first = []
    for flag, clip in expected:
        frames = [frame for frame, drawn, _, _, flags in rows if flag in flags and drawn == clip]
        if not frames:
            raise ValueError(f"Leap never drew {clip} while {flag} was set")
        first.append(frames[0])
    if first != sorted(first):
        raise ValueError(f"Leap clips drawn out of order at frames {first}")
    if rows[-1][4] & {"THROWN_PROJECTILE", "ABOUT_TO_HIT"}:
        raise ValueError("Leap flight conditions stayed set after landing")
    landing = [row for row in rows if row[1] == "mumollok_lepc" and "ABOUT_TO_HIT" not in row[4]]
    if not landing or landing[-1][2] != landing[-1][3] - 1:
        raise ValueError(f"Landing clip did not reach its last pose after landing: {landing[-1:]}")
