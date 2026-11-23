from math import dist
import re


render_env = {"PLAYTEST_STATIC_CAPTURES": "1"}

STEERING_ROW = re.compile(
    r"^TEST steering (\d+) (\S+) pos=(\S+) facing=\S+ goal=\S+ moving=([01])"
    r" velocity=\S+ loco=(\S+).*? objectMoving=([01]) planar=(\S+)",
    re.MULTILINE,
)


def check(log):
    rows = {
        (actor, int(frame)): (
            tuple(map(float, position.split(","))),
            ai == "1",
            locomotor.split(":")[0],
            moving == "1",
            tuple(map(float, planar.split(","))),
        )
        for frame, actor, position, ai, locomotor, moving, planar in STEERING_ROW.findall(log)
    }
    failures = []

    def sample(actor, frame):
        try:
            return rows[actor, frame]
        except KeyError as error:
            raise RuntimeError(f"{actor}: missing steering trace at frame {frame}") from error

    for actor, locomotor, target in (
        ("GondorImrahil", "HeroHorseLocomotor", (2525, 1020)),
        ("GondorPippin", "HeroHumanLocomotor", (2525, 1260)),
    ):
        start, ai, loco, moving, velocity = sample(actor, 1158)
        if ai or moving or velocity != (0, 0) or loco != locomotor:
            failures.append(f"{actor}: expected to start idle with {locomotor}")
        position, ai, _, moving, velocity = sample(actor, 1188)
        if not (ai and moving) or velocity == (0, 0) or position == start:
            failures.append(f"{actor}: did not move after the short order")
        position, ai, _, moving, velocity = sample(actor, 1232)
        if ai or moving or velocity != (0, 0) or dist(position, target) > 10:
            failures.append(f"{actor}: did not settle within one path cell by frame 1232 ({position}, {velocity})")

    actor = "GondorImrahil"
    _, ai, _, moving, velocity = sample(actor, 1290)
    if not (ai and moving) or velocity == (0, 0):
        failures.append(f"{actor}: Stop was issued without active motion")
    stopped = [(frame, sample(actor, frame)) for frame in range(1291, 1303)]
    resting = next((frame for frame, (_, ai, _, _, velocity) in stopped if not ai and velocity == (0, 0)), None)
    if resting is None or resting + 3 > stopped[-1][0]:
        failures.append(f"{actor}: no settled Stop with three observable frames afterward")
    elif any(ai or moving or velocity != (0, 0) for frame, (_, ai, _, moving, velocity) in stopped if frame >= resting + 3):
        failures.append(f"{actor}: MOVING survived more than three frames after Stop reached zero velocity")

    if failures:
        raise RuntimeError("\n".join(failures))
