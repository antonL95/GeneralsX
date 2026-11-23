import re
from collections import defaultdict

render_env = {"GENERALSX_TEST_STREAKTRACE": "GoodFactionArrow"}


def check(log):
    traces = defaultdict(dict)
    for frame, projectile, length in re.findall(
        r"^TEST streak frame (\d+) id (\d+) GoodFactionArrow longest ([\d.]+)$", log, re.MULTILINE,
    ):
        traces[projectile][int(frame)] = float(length)
    for samples in traces.values():
        if 55 not in samples or 75 not in samples or any(57 <= frame < 74 for frame in samples):
            continue
        reentry = 74 if 74 in samples else 75
        steady_frames = (51, 52, 53, 54, 55, 85, 86, 87)
        if not all(frame in samples for frame in steady_frames):
            continue
        nearby = [samples[frame] for frame in steady_frames]
        if min(nearby) <= 0 or not 0 < samples[reentry] <= 2 * max(nearby):
            raise RuntimeError(f"Reentry segment at frame {reentry} was {samples[reentry]}, nearby segments {nearby}")
        return
    spans = [[min(samples), max(samples), len(samples)] for samples in traces.values()]
    launches = re.findall(r"^TEST projectile frame=\d+ weapon=PlaytestStreakWeapon .*$", log, re.MULTILINE)
    raise RuntimeError(f"Expected one arrow across the offscreen interval, got {spans}.\n" + "\n".join(launches))
