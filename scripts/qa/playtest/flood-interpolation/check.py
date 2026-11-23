from collections import defaultdict
from math import dist, isclose
import re


render_env = {"GENERALSX_TEST_DRAWTRACE": "FloodHorse,FloodKotD"}

TRACE = re.compile(
    r"^TEST draw-position frame=(\d+) id=(\d+) template=(\S+) "
    r"drawn=([\d.,-]+) logic=([\d.,-]+)$",
    re.MULTILINE,
)
TICK_FRAMES = 6


def intervals(log, template):
    by_member = defaultdict(dict)
    for frame, member, name, drawn, logic in TRACE.findall(log):
        if name == template:
            by_member[int(member)][int(frame)] = (
                tuple(map(float, drawn.split(","))),
                tuple(map(float, logic.split(","))),
            )
    for member, samples in sorted(by_member.items()):
        for start in sorted(samples):
            frames = range(start - 1, start + TICK_FRAMES + 1)
            if not all(frame in samples for frame in frames):
                continue
            previous_logic = samples[start - 1][1]
            current_logic = samples[start][1]
            tick_distance = dist(previous_logic, current_logic)
            if tick_distance == 0 or samples[start + TICK_FRAMES][1] == current_logic:
                continue
            if any(samples[frame][1] != current_logic for frame in range(start, start + TICK_FRAMES)):
                continue
            steps = [dist(samples[frame - 1][0], samples[frame][0])
                     for frame in range(start, start + TICK_FRAMES)]
            yield member, start, tick_distance, steps


def check(log):
    failures = []
    for template in ("FloodHorse", "FloodKotD"):
        samples = list(intervals(log, template))
        if not samples:
            failures.append(f"{template}: no complete six-frame interval between simulation moves")
            continue
        for member, start, tick_distance, steps in samples:
            if min(steps) == 0 or max(steps) > tick_distance / 3 or not isclose(
                sum(steps), tick_distance, rel_tol=0.001
            ):
                steps_text = ", ".join(f"{step:.4f}" for step in steps)
                failures.append(
                    f"{template} {member} frame {start}: tick displacement {tick_distance:.4f}, "
                    f"draw steps [{steps_text}]"
                )
                break
    if failures:
        raise RuntimeError("\n".join(failures))
