import re

from PIL import Image, ImageStat


def check(log):
    cameras = {
        int(frame): tuple(map(float, values.split()))
        for frame, values in re.findall(r"TEST hud-camera (\d+) ([^\n]+)", log)
    }
    ground = cameras[119]
    if abs(ground[2]) > 0.01 or abs(ground[3] - 0.7) > 0.01:
        raise AssertionError(f"Camera did not enter Ground View: {cameras}")
    missing_sky = re.findall(r"\[TEX_MISSING\] '([^']*sky[^']*)'", log, re.IGNORECASE)
    if missing_sky:
        raise AssertionError(f"Missing sky textures: {missing_sky}")


def check_captures(output, _frames):
    with Image.open(output / "frame-0119.png") as image:
        sky = image.convert("RGB").crop((120, 0, 1280, 220))
        pixels = list(sky.getdata())
        magenta = sum(r >= 150 and b >= 150 and g <= 80 and min(r, b) - g >= 90 for r, g, b in pixels)
        if magenta > 0:
            raise AssertionError(f"Ground View upper sky has {magenta} magenta pixels")
        stat = ImageStat.Stat(sky)
        mean_luma = sum(stat.mean) / 3.0
        max_stddev = max(stat.stddev)
        if mean_luma < 25.0 or max_stddev < 5.0:
            raise AssertionError(
                f"Ground View upper sky lacks textured sky pixels (mean={mean_luma:.1f}, stddev={max_stddev:.1f})"
            )
