import re

from PIL import Image


def check(log):
    for pattern in (
        r"TEST frame 24: deselect all",
        r"TEST frame 32: select object \d+ \(GondorGandalf\)",
        r"TEST frame 40: select object \d+ \(LamedonClansmenHorde\)",
    ):
        if not re.search(pattern, log):
            raise RuntimeError(f"Missing palantir selection state: {pattern}")


def hud_pixels(image, left, top, right, bottom):
    scale = min(image.width / 1024, image.height / 768)
    bounds = (round(left * scale), image.height - round((300 - top) * scale),
              round(right * scale), image.height - round((300 - bottom) * scale))
    return list(image.crop(bounds).convert("RGB").getdata())


def check_captures(output, frames):
    failures = []
    measurements = []
    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            label = f"{image.width}x{image.height} frame {frame}"
            edge = hud_pixels(image, 207, 163, 213, 189)
            parchment = sum(red >= 28 and red - blue >= 18 for red, _, blue in edge) / len(edge)
            rim = hud_pixels(image, 103, 49, 148, 58)
            silver = sum(min(rgb) >= 120 and max(rgb) - min(rgb) <= 16 for rgb in rim) / len(rim)
            measurements.append(f"{label}: {parchment:.1%} parchment, {silver:.1%} silver")
            if parchment < 0.75:
                failures.append(f"{label}: palantir overlaps the radar ({parchment:.0%} parchment)")
            if silver < 0.25:
                failures.append(f"{label}: Gondor rim is missing ({silver:.0%} silver)")
    if failures:
        raise RuntimeError("; ".join(failures))
    (output / "radar-coverage.txt").write_text("\n".join(measurements) + "\n")
