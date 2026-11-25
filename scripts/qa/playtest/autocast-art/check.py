import re

from PIL import Image


def buttons(log):
    found = {}
    pattern = r"TEST autocast-art frame=(\d+) slot=(\d) availability=(-?\d+) auto=(\d) x=(\d+) y=(\d+) width=(\d+) height=(\d+)"
    for row in re.findall(pattern, log):
        frame, slot, availability, auto, x, y, width, height = map(int, row)
        found[(frame, slot)] = {"availability": availability, "auto": auto, "box": (x, y, width, height)}
    return found


def check(log):
    state = buttons(log)
    if state[(30, 3)]["availability"] != 0 or state[(30, 4)]["availability"] != 0:
        raise RuntimeError(f"Expected two restricted ability buttons on the first hero: {state}")
    if state[(38, 2)]["auto"] != 0 or state[(50, 2)]["auto"] != 1:
        raise RuntimeError(f"Right click did not switch autocast on: {state}")
    check.state = state


def luma(pixel):
    return (299 * pixel[0] + 587 * pixel[1] + 114 * pixel[2]) // 1000


def inner_pixels(image, box):
    x, y, width, height = box
    margin = width // 4
    crop = image.convert("RGB").crop((x + margin, y + margin, x + width - margin, y + height - margin))
    return list(crop.getdata())


def annulus_luma(image, box):
    x, y, width, height = box
    cx, cy = x + width / 2, y + height / 2
    pixels = image.convert("RGB")
    values = {}
    for py in range(int(cy - width), int(cy + width)):
        for px in range(int(cx - width), int(cx + width)):
            distance = ((px - cx) ** 2 + (py - cy) ** 2) ** 0.5
            if width * 0.52 <= distance <= width * 0.95:
                values[(px, py)] = luma(pixels.getpixel((px, py)))
    return values


def check_captures(output, _frames):
    state = check.state

    def load(frame):
        return Image.open(output / f"frame-{frame:04d}.png")

    for slot in (3, 4):
        disabled = inner_pixels(load(34), state[(30, slot)]["box"])
        brightest = sorted(luma(p) for p in disabled)[-max(1, len(disabled) // 10):]
        if sum(brightest) / len(brightest) < 100:
            raise RuntimeError(f"Unavailable button {slot} is darkened instead of plain grayscale")
        spread = sum(max(p) - min(p) for p in disabled) / len(disabled)
        if spread > 12:
            raise RuntimeError(f"Unavailable button {slot} is tinted, colour spread {spread:.1f}")

    box = state[(50, 2)]["box"]
    before = annulus_luma(load(40), box)
    shots = [annulus_luma(load(frame), box) for frame in (54, 68, 82)]

    def bright(values):
        return sum(v > 170 for v in values.values())

    counts = [bright(shot) for shot in shots]
    if max(counts) < bright(before) + 40:
        raise RuntimeError(f"No sparkles around the autocast button: before {bright(before)}, after {counts}")
    moved = sum(abs(shots[0][k] - shots[1][k]) > 40 for k in shots[0])
    if moved < 8:
        raise RuntimeError(f"The autocast overlay is not animated, {moved} changed pixels")
