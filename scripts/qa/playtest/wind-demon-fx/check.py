import re


render_env = {"GENERALSX_TEST_DRAWTRACE": "WindDemon"}

DRAW_LINE = re.compile(
    r"^TEST draw frame (\d+) id (\d+) WindDemon module \S+ model floodhorse_skn .* subobjects (\S+)$",
    re.MULTILINE,
)


def check(log):
    matches = DRAW_LINE.findall(log)
    if not matches:
        raise RuntimeError("No WindDemon draw trace entries found")
    shown = [
        f"frame {frame} id {member}: {subobjects}"
        for frame, member, subobjects in matches
        if "FLOODHORSE_SKN.HORSE:hidden" not in subobjects or "FLOODHORSE_SKN.HORSE:shown" in subobjects
    ]
    if shown:
        raise RuntimeError(f"WindDemon horse subobject was not hidden ({len(shown)}/{len(matches)}): {shown[0]}")
