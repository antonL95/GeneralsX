import math
import re

from PIL import Image, ImageChops, ImageStat


render_env = {"GENERALSX_TEST_PARTICLE_MODEL": "1", "PLAYTEST_STATIC_CAPTURES": "1"}


def check(log):
    traces = [(int(frame), name, float(x), float(y)) for frame, name, x, y in re.findall(
        r"TEST particle-model (\d+) (\S+) y-axis=([\d.-]+),([\d.-]+),[\d.-]+",
        log,
    )]
    for first, last, particle, target in (
        (40, 146, "GandalfIstariLightRays", (110, 0)),
        (160, 238, "EvilEyeLightRaysAotR", (110, 0)),
        (245, 312, "GandalfIstariLightRays", (0, 110)),
    ):
        scores = [-(x * target[0] + y * target[1]) / (math.hypot(x, y) * math.hypot(*target))
                  for frame, name, x, y in traces if first <= frame <= last and name == particle]
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
