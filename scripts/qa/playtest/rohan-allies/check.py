import re
import sys

from PIL import Image, ImageChops, ImageFilter


render_variants = {"pinned-very-high": {"PLAYTEST_PIN_DYNAMIC_LOD": "1"}}


def check(log):
    samples = {int(frame): (int(count), int(youngest), int(oldest), float(alpha))
               for frame, count, youngest, oldest, alpha in re.findall(
                   r"^TEST particle-stats frame=(\d+) name=RohirimAlliesDust01 count=(\d+) "
                   r"youngest=(\d+) oldest=(\d+) peak-alpha=[\d.]+ total-alpha=([\d.]+)$",
                   log, re.MULTILINE)}
    for frame, minimum, max_youngest, min_alpha in ((105, 8, 15, 2.0), (184, 8, 57, 1.8), (214, 3, 86, 0.25)):
        count, youngest, oldest, alpha = samples.get(frame, (0, 0, 0, 0.0))
        if count < minimum or youngest > max_youngest or oldest > 100 or alpha < min_alpha:
            raise RuntimeError(
                f"Summon dust at frame {frame}: {count} particles, ages {youngest}–{oldest}, total alpha {alpha:.2f}")
    if not re.search(r"^TEST particle-systems frame=274 name=RohirimAlliesDust01 systems=0 particles=0\b",
                     log, re.MULTILINE):
        raise RuntimeError("Summon dust did not expire by frame 274")


def check_captures(output, frames):
    def dust_crop(frame):
        with Image.open(output / f"frame-{frame:04}.png") as image:
            return image.crop((570, 390, 800, 500)).convert("L").filter(ImageFilter.GaussianBlur(5))

    cleared = dust_crop(244)
    for frame, threshold, minimum in ((184, 15, 8000), (214, 12, 1000)):
        brighter = ImageChops.subtract(dust_crop(frame), cleared)
        area = sum(brighter.histogram()[threshold:])
        if area < minimum:
            raise RuntimeError(f"Summon dust at frame {frame}: only {area} brightened crop pixels")
        print(f"Summon dust at frame {frame}: {area} brightened crop pixels", file=sys.stderr)
