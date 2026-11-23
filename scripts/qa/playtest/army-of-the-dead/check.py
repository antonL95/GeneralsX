from collections import defaultdict
from itertools import pairwise
import re
from statistics import correlation
import sys

from PIL import Image, ImageFilter


def check(log):
    samples = defaultdict(list)
    for match in re.finditer(
        r"^TEST particle-live frame=(\d+) id=\d+ name=(\S+) shader=(\d+) texture=(\S+) "
        r"count=([1-9]\d*) age=(\d+) rgb=([\d.,]+) alpha=([\d.]+) ",
        log,
        re.MULTILINE,
    ):
        frame, name, shader, texture, count, age, rgb, alpha = match.groups()
        samples[int(frame), name].append(
            (int(shader), texture, int(count), int(age), tuple(map(float, rgb.split(","))), float(alpha))
        )

    if any(frame == 233 and name.startswith("Resurrection") for frame, name in samples):
        raise RuntimeError("Elessar's spawn effects are still active during the cast capture")

    flare = samples[233, "SpookyOathBreakerFlare"]
    if not any(shader == 1 and texture == "EXLnzFlar2.tga" and 0 < red < blue < green <= 31 / 255
               for shader, texture, _, _, (red, green, blue), _ in flare):
        raise RuntimeError("Expected an additive cast flare with its authored green colour")

    mist = samples[233, "NecromancerGreenMistAroundFeetFX"]
    if not any(shader == 2 and texture == "EXsmokeplume.tga" and 0 < alpha <= 0.15
               for shader, texture, _, _, _, alpha in mist):
        raise RuntimeError("Expected the King's alpha-blended feet mist")

    trail = samples[280, "AODTrails"]
    if len(trail) < 10 or not any(
        shader == 1 and texture == "EXFire01.tga" and 0 < red < blue < green <= 0.079
        for shader, texture, _, _, (red, green, blue), _ in trail
    ):
        raise RuntimeError("Expected active additive trails with their authored green colour")

    shots = defaultdict(list)
    for frame, owner in re.findall(
        r"^TEST timed weapon frame=(\d+) source=(\d+)\(FloodOathbreakerCOM\) ", log, re.MULTILINE
    ):
        shots[owner].append(int(frame))
    if len(shots) != 5 or not all(any(b == a + 1 for a, b in pairwise(frames)) for frames in shots.values()):
        raise RuntimeError("Expected all five commanders to fire their continuous weapon")

    effects = defaultdict(list)
    for owner, frame in re.findall(
        r"^\[Retail FXList\] Sound NoseyOathbreakerVoxSelect owner=(\d+) action=start frame=(\d+)$",
        log,
        re.MULTILINE,
    ):
        if owner in shots:
            effects[owner].append(int(frame) // 6)
    for owner in shots:
        ticks = effects[owner]
        if len(ticks) < 2 or len(ticks) != len(set(ticks)):
            raise RuntimeError(f"Expected one weapon FX playback per retail tick for commander {owner}")


def crop(output, frame, area):
    with Image.open(output / f"frame-{frame:04d}.png") as image:
        return image.convert("RGB").crop(area)


def detail(image):
    luma = image.convert("L")
    smooth = luma.filter(ImageFilter.GaussianBlur(2))
    return [sharp - soft for sharp, soft in zip(luma.get_flattened_data(), smooth.get_flattened_data())]


def check_captures(output, frames):
    for frame, area in (
        (233, (725, 350, 850, 420)),
        (280, (845, 322, 1045, 460)),
        (340, (990, 323, 1230, 482)),
    ):
        ground = crop(output, 180, area)
        active = crop(output, frame, area)
        pixels = active.get_flattened_data()
        white = sum(min(rgb) >= 210 and max(rgb) - min(rgb) <= 12 for rgb in pixels) / len(pixels)
        green = 0
        for (r, g, b), (gr, gg, gb) in zip(pixels, ground.get_flattened_data()):
            dr, dg, db = r - gr, g - gg, b - gb
            green += dg >= 8 and dg >= dr + 3 and dg >= db - 5
        green /= len(pixels)
        active_detail = detail(active)
        terrain = correlation(detail(ground), active_detail) if max(active_detail) != min(active_detail) else 0
        print(f"Frame {frame} crop: white={white:.3f}, green={green:.3f}, terrain={terrain:.3f}", file=sys.stderr)
        if white >= 0.04 or green <= 0.20 or terrain <= 0.45:
            raise RuntimeError(
                f"Expected green FX over legible terrain at frame {frame}, "
                f"got white={white:.3f}, green={green:.3f}, terrain={terrain:.3f}"
            )
