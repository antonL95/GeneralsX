from math import hypot
import re
from statistics import mean
import sys

from PIL import Image


render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "GENERALSX_TRACE_RETAIL_PARTICLE_SPAWNS": "1",
}


def check(log):
    if "[RetailFX] Drew GlowEffect" not in log:
        raise RuntimeError("The model glow pass did not draw")
    if not re.search(r"TEST frame 68: objupgrade object \d+ Upgrade_HallOfFireGlowFixture given", log):
        raise RuntimeError("Gil-galad did not receive the fixture weapon-set upgrade")
    spawns = [
        int(frame)
        for frame in re.findall(r"\[Retail Particle\] frame=(\d+) id=\d+ name=HoFUnitTrails\b", log)
    ]
    if not any(20 <= frame < 40 for frame in spawns):
        raise RuntimeError(f"The default spirit state did not create its aura. Spawns {spawns}")
    if not any(68 <= frame < 100 for frame in spawns):
        raise RuntimeError(f"The upgraded spirit state did not recreate its aura. Spawns {spawns}")

    samples = re.findall(
        r"TEST particle-bone frame=(\d+) name=HoFUnitTrails attached=\d+ x=([\d.]+) y=([\d.]+)", log
    )
    for frame in (60, 120):
        if not any(int(f) == frame and hypot(float(x) - 3055, float(y) - 900) <= 25 for f, x, y in samples):
            raise RuntimeError(f"No aura particle near Gil-galad at frame {frame}. Samples {samples}")


def pixels(output, frame, box):
    with Image.open(output / f"frame-{frame:04d}.png") as capture:
        return capture.convert("RGB").crop(box).get_flattened_data()


def check_captures(output, frames):
    halo_crop = (360, 195, 400, 345)
    aura_crop = (375, 345, 400, 400)
    halo_luma = {
        frame: mean((r + g + b) / 3 for r, g, b in pixels(output, frame, halo_crop))
        for frame in frames
    }
    aura_motion = {}
    for first, last in zip(frames[::2], frames[1::2]):
        aura_motion[last] = sum(
            abs(r - pr) >= 12 and abs((r - b) - (pr - pb)) >= 6 and max(r - b, pr - pb) >= 25
            for (r, g, b), (pr, pg, pb) in zip(
                pixels(output, first, aura_crop), pixels(output, last, aura_crop)
            )
        )
    print(f"Spirit halo crop {halo_crop}, mean RGB {halo_luma}", file=sys.stderr)
    print(f"Spirit aura crop {aura_crop}, changing warm pixels {aura_motion}", file=sys.stderr)
    if any(value < 75 for value in halo_luma.values()):
        raise RuntimeError(f"The model halo did not brighten the crop on every frame. Mean RGB {halo_luma}")
    if any(count < 150 for count in aura_motion.values()):
        raise RuntimeError(f"The foot aura did not animate in both model states. Changing pixels {aura_motion}")
