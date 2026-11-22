import re
from collections import defaultdict


def check(log):
    counts = defaultdict(list)
    for frame, name, count in re.findall(
        r"^TEST particle-systems frame=(\d+) name=(AODLightShafts|MoMLightShafts) systems=(\d+) ",
        log, re.MULTILINE,
    ):
        counts[int(frame), name].append(int(count))

    for name in ("AODLightShafts", "MoMLightShafts"):
        if counts[32, name] != [5]:
            raise RuntimeError(f"Expected five initial plays of {name}, found {counts[32, name]}")

    for frame, expected_additions in ((180, 0), (181, 1)):
        samples = counts[frame, "MoMLightShafts"]
        if len(samples) != 2 or samples[1] - samples[0] != expected_additions:
            raise RuntimeError(f"Expected {expected_additions} new plays at frame {frame}, found {samples}")
