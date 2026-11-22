import re


def check(log):
    counts = {(int(frame), name): int(count) for frame, name, count in
              re.findall(r"^TEST particle-systems frame=(\d+) name=(AODLightShafts|MoMLightShafts) systems=(\d+) ",
                         log, re.MULTILINE)}
    expected = {(32, "AODLightShafts"): 5, (32, "MoMLightShafts"): 5,
                (180, "MoMLightShafts"): 5, (181, "MoMLightShafts"): 6}
    if counts != expected:
        raise RuntimeError(f"Expected five plays through the tracking window and a sixth after it, found {counts}")
