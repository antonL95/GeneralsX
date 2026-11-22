import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "LAGondorFighter,LAGondorArcher"}


def check(log):
    pattern = (r"^TEST draw frame (\d+) id \d+ (LAGondorFighter|LAGondorArcher) "
               r"module \S+ model \S+ clip (\S+) at ([\d.]+)/[1-9]\d* .*"
               r" flags \S*\bHIT_REACTION\b")
    samples = {(int(frame), name): (clip, float(pose))
               for frame, name, clip, pose in re.findall(pattern, log, re.MULTILINE)}
    for actor, should_restart in (("LAGondorFighter", True), ("LAGondorArcher", False)):
        before = samples.get((38, actor))
        after = samples.get((40, actor))
        later = samples.get((45, actor))
        if not before or not after or not later or not (before[0] == after[0] == later[0]):
            raise RuntimeError(f"{actor}: expected the same hit clip at each sample, found {before}, {after}, {later}")
        if (after[1] < before[1]) != should_restart or later[1] <= after[1]:
            raise RuntimeError(f"{actor}: expected restart={should_restart}, found poses {before[1]}, {after[1]}, {later[1]}")
