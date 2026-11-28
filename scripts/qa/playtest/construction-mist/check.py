import re

from PIL import Image


def check(log):
    match = re.search(r"TEST particle-systems frame=145 name=BuildingContructDust systems=(\d+) particles=(\d+)", log)
    if not match or int(match.group(1)) < 1 or int(match.group(2)) < 1:
        raise RuntimeError("BuildingContructDust did not emit particles during construction at frame 145")


def check_captures(output, _frames):
    with Image.open(output / "frame-0110.png") as before, Image.open(output / "frame-0150.png") as after:
        before_rgb = before.convert("RGB")
        after_rgb = after.convert("RGB")
        bp = before_rgb.load()
        ap = after_rgb.load()

        x0, x1 = 560, 860
        y0, y1 = 360, 446
        visible_pixels = 0
        sharp_drops = []
        for x in range(x0, x1):
            col = [max(abs(ap[x, y][c] - bp[x, y][c]) for c in range(3)) for y in range(y0, y1 + 3)]
            for idx in range(len(col) - 2):
                d0 = col[idx]
                if d0 >= 12:
                    visible_pixels += 1
                if d0 >= 18 and col[idx + 1] <= 2 and col[idx + 2] <= 2:
                    sharp_drops.append((x, y0 + idx, d0, col[idx + 1]))

        if visible_pixels < 300:
            raise RuntimeError(f"Expected active construction dust region, saw only {visible_pixels} mist pixels")
        if sharp_drops:
            sample = ", ".join(f"(x={x},y={y}:{d0}->{d1})" for x, y, d0, d1 in sharp_drops[:6])
            raise RuntimeError(f"Sharp vertical terrain cut detected in construction mist ({len(sharp_drops)} cols): {sample}")
