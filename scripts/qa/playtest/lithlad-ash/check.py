import re
from statistics import median

from PIL import Image, ImageChops, ImageDraw, ImageFilter

CROP = (384, 56, 1200, 544)
SAMPLES = {"start": (92, 152, 182), "far": (212, 272, 332, 512, 752, 992)}
EXCLUSIONS = {
    "start": ((0, 0, 815, 6), (115, 110, 490, 372), (390, 368, 601, 435), (750, 0, 782, 16)),
    "far": ((0, 0, 815, 6), (130, 146, 462, 366), (366, 366, 552, 429), (674, 16, 708, 53)),
}
THRESHOLD = 12
MIN_MEDIAN_COVERAGE = {"start": 13, "far": 24}
HIDDEN_FRAMES = tuple(
    control for samples in SAMPLES.values() for frame in samples for control in (frame - 1, frame + 1)
)
render_env = {
    "PLAYTEST_FACTION": "FactionRohan",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "PLAYTEST_HIDE_WEATHER_FRAMES": " ".join(map(str, HIDDEN_FRAMES)),
    "GENERALSX_TEST_WEATHER_DRAW": "1",
}


def load_crop(output, frame):
    with Image.open(output / f"frame-{frame:04d}.png") as image:
        return image.convert("RGB").crop(CROP)


def ground_mask(size, view):
    mask = Image.new("L", size, 255)
    draw = ImageDraw.Draw(mask)
    for box in EXCLUSIONS[view]:
        draw.rectangle(box, fill=0)
    return mask


def measure_ash(sample, controls, mask, *, retail=False):
    low = high = controls[0]
    for control in controls[1:]:
        low = ImageChops.darker(low, control)
        high = ImageChops.lighter(high, control)
    motion = ImageChops.subtract(high, low)
    excess = ImageChops.subtract(sample, high)
    width, _ = sample.size
    local_excess = excess.convert("L").filter(ImageFilter.GaussianBlur(4))
    points = {
        (index % width, index // width): max(delta)
        for index, (delta, background, nearby, include) in enumerate(zip(
            excess.get_flattened_data(), motion.get_flattened_data(),
            local_excess.get_flattened_data(), mask.get_flattened_data()
        ))
        if include and max(delta) >= THRESHOLD
        and (nearby <= 5 if retail else max(background) <= 2)
    }
    flecks = []
    while points:
        origin, peak = points.popitem()
        pixels = [origin]
        for x, y in pixels:
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbor = (x + dx, y + dy)
                    if neighbor in points:
                        peak = max(peak, points.pop(neighbor))
                        pixels.append(neighbor)
        xs, ys = zip(*pixels)
        bounds = min(xs), min(ys), max(xs), max(ys)
        left, top, right, bottom = bounds
        width_px, height_px = right - left + 1, bottom - top + 1
        if len(pixels) >= 2 and width_px * height_px <= 120:
            flecks.append((len(pixels), width_px, height_px, peak, bounds))
    return sorted(flecks, reverse=True)


def check(log):
    measurements = {}
    for match in re.finditer(r"TEST weather-draw frame=(\d+) .*? enabled=(\d) .*? count=(\d+)", log):
        frame, enabled, count = map(int, match.groups())
        measurements.setdefault(frame, set()).add((enabled, count > 0))
    for frame in HIDDEN_FRAMES:
        if measurements.get(frame) != {(0, False)}:
            raise RuntimeError(f"Lithlad ash control drew weather at frame {frame}")
    for samples in SAMPLES.values():
        for frame in samples:
            if measurements.get(frame) != {(1, True)}:
                raise RuntimeError(f"Lithlad ash draw missing at frame {frame}")


def check_captures(output, _frames):
    lines = []
    for view, samples in SAMPLES.items():
        coverage_by_frame = []
        for frame in samples:
            sample = load_crop(output, frame)
            controls = [load_crop(output, control) for control in (frame - 1, frame + 1)]
            mask = ground_mask(sample.size, view)
            area = mask.histogram()[255]
            flecks = measure_ash(sample, controls, mask)
            coverage = sum(fleck[0] for fleck in flecks)
            coverage_by_frame.append(coverage)
            lines.append(
                f"{view} frame={frame} area={area} flecks={len(flecks)} "
                f"density={len(flecks) * 1e5 / area:.2f}/100kpx coverage={coverage} "
                f"pixels={flecks}"
            )
        early_coverage = median(coverage_by_frame[:3])
        if early_coverage < MIN_MEDIAN_COVERAGE[view]:
            raise RuntimeError(
                f"Lithlad {view} ash median coverage {early_coverage:g} pixels "
                f"below {MIN_MEDIAN_COVERAGE[view]} in frames {samples[:3]}"
            )
    (output / "ash-metrics.txt").write_text("\n".join(lines) + "\n")
