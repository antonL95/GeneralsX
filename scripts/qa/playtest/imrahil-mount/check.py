import re

from PIL import Image, ImageChops, ImageStat


IMRAHIL = "GondorImrahil"
CROP = (565, 215, 750, 350)
render_env = {"GENERALSX_TEST_DRAWTRACE": f"{IMRAHIL},RohanEomer,RohanTheoden"}

DRAW_ROW = re.compile(
    r"^TEST draw frame (\d+) id \d+ (\S+) module \S+ model (\S+) clip .*?"
    r" flags (\S*) subobjects (\S+)$",
    re.MULTILINE,
)


def check(log):
    if not re.search(r"^TEST frame 7: queue GondorImrahil on object \d+$", log, re.MULTILINE):
        raise RuntimeError("Imrahil was not queued at the citadel")

    draws = {
        (int(frame), actor): (model, flags.split(","), subobjects.split(","))
        for frame, actor, model, flags, subobjects in DRAW_ROW.findall(log)
        if model != "-"
    }
    for frame, actor, expected_model, mounted, moving in (
        (1599, IMRAHIL, "guimrahrs_skn", True, False),
        (1640, IMRAHIL, "guimrahrs_skn", True, True),
        (1680, IMRAHIL, "guimrahrs_skn", True, False),
        (1818, IMRAHIL, "gunimrafh_skn", False, False),
        (1892, IMRAHIL, "guimrahrs_skn", True, False),
        (1910, IMRAHIL, "guimrahrs_skn", True, True),
        (1595, "RohanEomer", "runeomer_mnt", True, False),
        (1595, "RohanTheoden", "rutheom_light", True, False),
    ):
        state = draws.get((frame, actor))
        if state is None:
            raise RuntimeError(f"{actor} draw trace missing at frame {frame}")
        model, flags, subobjects = state
        if model != expected_model or ("MOUNTED" in flags) != mounted:
            raise RuntimeError(f"{actor} frame {frame} drew {model} with flags {flags}, expected {expected_model}")
        if ("MOVING" in flags) != moving:
            raise RuntimeError(f"{actor} frame {frame} expected moving={moving}, flags={flags}")
        if actor == IMRAHIL and mounted and "GUIMRAHRS_SKN.HORSE:shown" not in subobjects:
            raise RuntimeError(f"Imrahil's horse mesh was not shown at frame {frame}")


def check_captures(output, _frames):
    def crop(frame):
        with Image.open(output / f"frame-{frame:04d}.png") as capture:
            return capture.crop(CROP).convert("RGB")

    def distance(left, right):
        return sum(ImageStat.Stat(ImageChops.difference(left, right)).mean) / 3

    recruited, dismounted, remounted = (crop(frame) for frame in (1680, 1818, 1892))
    to_foot = distance(recruited, dismounted)
    to_horse = distance(recruited, remounted)
    if to_foot < 5 or to_foot <= to_horse:
        raise RuntimeError(f"Recruit crop differs from dismount by {to_foot:.2f} and remount by {to_horse:.2f}")
