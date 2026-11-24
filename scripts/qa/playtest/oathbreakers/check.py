import math
import re
from statistics import median
import sys


render_env = {
    "GENERALSX_TEST_DRAWTRACE": (
        "RohanOathbreaker1,RohanOathbreaker2,RohanOathbreaker3,"
        "FloodOathbreaker,FloodOathbreakerCOM,FloodKotD"
    ),
}


def check(log):
    for cast in ("cast Command_SpawnOathbreakers at point (3100.0, 690.0)",
                 "aim Command_ElessarOathbreakerFlood at point (3220.0, 850.0)"):
        if cast not in log:
            raise RuntimeError(f"Missing authored cast: {cast}")

    for system in ("OathBreakerSmokeGlow", "SpookyOathBreakerSmokeGlow"):
        if not re.search(rf"^TEST particle-systems .* name={system} systems=[1-9]\d* ", log, re.MULTILINE):
            raise RuntimeError(f"Missing authored summon FX: {system}")

    model = re.search(r"^TEST draw frame 218 id (\d+) FloodOathbreaker .* model ruoath3a_skn "
                      r"clip ruoath3_runa ", log, re.MULTILINE)
    if not model:
        raise RuntimeError("Expected the authored flood model and run clip before fading")

    drawable_id = model.group(1)
    for frame, expected in ((218, 1.0), (226, 12 / 18), (236, 2 / 18), (238, 0.0)):
        draw = re.search(rf"^TEST draw-opacity frame {frame} id {drawable_id} .* effective ([\d.]+) "
                         r"ocl 1\.000000 material 1\.000000 animation 1\.000000 sorted \d+ blend 1/1$",
                         log, re.MULTILINE)
        if not draw or not math.isclose(float(draw.group(1)), expected, abs_tol=1e-6):
            raise RuntimeError(f"Expected additive draw at frame {frame} with effective opacity {expected:.6f}")

    if not re.search(rf"^TEST draw frame 236 id {drawable_id} .* flags RUBBLE ", log, re.MULTILINE):
        raise RuntimeError("Expected the ghost's authored slow death to retain its draw model")


def check_captures(output, frames):
    from PIL import Image, ImageChops

    def crop(frame):
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            return image.convert("RGB").crop((580, 255, 700, 385))

    ground = crop(240)

    def light(frame):
        delta = ImageChops.subtract(crop(frame), ground)
        return sum((index % 256) * count for index, count in enumerate(delta.histogram()))

    full, late, gone = (light(frame) for frame in (218, 236, 238))
    if not (full >= 2000 and 0 < late < full / 3 and gone < full / 10):
        raise RuntimeError(f"Expected the additive silhouette to fade over the terrain, got {full=}, {late=}, {gone=}")

    figure_area = (925, 81, 961, 126)
    with Image.open(output / "frame-0040.png") as before, Image.open(output / "frame-0146.png") as sweep:
        ground_light = before.crop(figure_area).convert("L").get_flattened_data()
        figure_light = sweep.crop(figure_area).convert("L").get_flattened_data()
    brightness = median(figure - ground for figure, ground in zip(figure_light, ground_light))
    print(f"Frame 146 ghost crop median luma gain: {brightness:.1f}", file=sys.stderr)
    if brightness < 10:
        raise RuntimeError(f"Expected the mid-sweep figure to brighten the terrain, got luma gain {brightness:.1f}")
