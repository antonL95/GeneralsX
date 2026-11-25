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


def ring_pixels(image, box, min_ratio, max_ratio):
    x, y, width, height = box
    cx, cy = x + width / 2, y + height / 2
    pixels = image.convert("RGB")
    values = {}
    for py in range(int(cy - width), int(cy + width)):
        for px in range(int(cx - width), int(cx + width)):
            distance = ((px - cx) ** 2 + (py - cy) ** 2) ** 0.5
            if width * min_ratio <= distance <= width * max_ratio:
                values[(px, py)] = pixels.getpixel((px, py))
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
    before_center = ring_pixels(load(40), box, 0.0, 0.30)
    before_rim = ring_pixels(load(40), box, 0.38, 0.72)
    rim_shots = []
    for frame in (54, 68, 82):
        image = load(frame)
        center = ring_pixels(image, box, 0.0, 0.30)
        center_delta = sum(luma(center[k]) - luma(before_center[k]) for k in center) / len(center)
        if center_delta > 30:
            raise RuntimeError(f"Autocast overlay obscures button center on frame {frame}: delta {center_delta:.1f}")
        rim = ring_pixels(image, box, 0.38, 0.72)
        rim_shots.append(rim)
        bright = [rim[k] for k in rim if luma(rim[k]) - luma(before_rim[k]) > 40]
        if len(bright) < 100:
            raise RuntimeError(f"Too few brightened rim pixels on frame {frame}: {len(bright)}")
        mean_r = sum(p[0] for p in bright) / len(bright)
        mean_g = sum(p[1] for p in bright) / len(bright)
        mean_b = sum(p[2] for p in bright) / len(bright)
        if mean_g < mean_r or (mean_r - mean_b) > 45:
            raise RuntimeError(f"Autocast rim glow has wrong colour on frame {frame}: ({mean_r:.0f},{mean_g:.0f},{mean_b:.0f})")

    moved = sum(abs(luma(rim_shots[0][k]) - luma(rim_shots[1][k])) > 40 for k in rim_shots[0])
    if moved < 40:
        raise RuntimeError(f"The autocast rim sparkles are not orbiting, {moved} changed pixels")
