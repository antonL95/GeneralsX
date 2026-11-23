import math
import re

from PIL import Image, ImageChops, ImageStat


render_env = {"GENERALSX_TEST_PARTICLE_MODEL": "1", "PLAYTEST_STATIC_CAPTURES": "1"}


def check(log):
    vector = r"([\d.-]+,[\d.-]+,[\d.-]+)"
    targets = {system: tuple(map(float, direction.split(","))) for system, direction in re.findall(
        rf"TEST particle-aim id=(\d+) direction={vector}", log
    )}
    traces = [(int(frame), name, system, tuple(map(float, axis.split(","))))
              for frame, name, system, axis in re.findall(
                  rf"TEST particle-model (\d+) (\S+) id=(\d+) y-axis={vector}", log
              )]
    for first, last, particle in (
        (40, 146, "GandalfIstariLightRays"),
        (160, 238, "EvilEyeLightRaysAotR"),
        (245, 312, "GandalfIstariLightRays"),
    ):
        scores = []
        for frame, name, system, axis in traces:
            if first <= frame <= last and name == particle:
                target = targets[system]
                scores.append(-sum(a * b for a, b in zip(axis, target)) / (math.hypot(*axis) * math.hypot(*target)))
        if not scores:
            raise RuntimeError(f"{particle} emitted no drawable shaft in frames {first}–{last}")
        if min(scores) < 0.9:
            raise RuntimeError(f"{particle} shaft tail missed its target: {min(scores):.3f}")


def check_captures(output, frames):
    with Image.open(output / "frame-0100.png") as windup, Image.open(output / "frame-0121.png") as release:
        brighter = ImageChops.subtract(release.convert("L"), windup.convert("L"))
        target_side = ImageStat.Stat(brighter.crop((570, 280, 1000, 410))).mean[0]
        rear_side = ImageStat.Stat(brighter.crop((75, 280, 505, 410))).mean[0]
        if target_side < 10 or target_side <= rear_side * 2:
            raise RuntimeError(f"Istari Light crop brightened by {target_side:.1f} toward the target and {rear_side:.1f} behind")
