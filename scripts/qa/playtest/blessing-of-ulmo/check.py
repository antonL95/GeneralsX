import re
from operator import sub
from pathlib import Path
from statistics import median

from PIL import Image, ImageChops, ImageStat


CAST_FRAME = 37
PRECAST_FRAME = 35
FIRST_RAIN_DRAW = CAST_FRAME + 1
RAIN_END = CAST_FRAME + 180 * 30
DRY_FRAME = RAIN_END + 103
TERRAIN_CROP = (780, 45, 1240, 295)
CAPTURE_FRAMES = tuple(map(int, Path(__file__).with_name("frames.txt").read_text().split()))
HIDDEN_FRAMES = (184, 185, 187, 188)

render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "PLAYTEST_HIDE_WEATHER_FRAMES": " ".join(map(str, HIDDEN_FRAMES)),
    "GENERALSX_TEST_WEATHER_DRAW": "1",
}


def check(log):
    if not re.search(r"^TEST powerstate 30 .* book=ArnorSpellBook ", log, re.MULTILINE):
        raise RuntimeError("The local player did not get Rivendell's authored spellbook")
    if "TEST list 38 Player_1 UlmoRainWeatherChanger 0 " not in log:
        raise RuntimeError("The spellbook cast did not create the authored weather changer")

    trace = re.compile(
        r"^TEST weather-draw frame=(\d+) state=(\d+) end=(\d+) enabled=(\d+) visible=(\d+) "
        r"map=(\d+) spell=(\d+) system=snow-manager count=(\d+) texture=(\S+) "
        r"authored=(\S+) spacing=([\d.]+) speed=([\d.]+)$",
        re.MULTILINE,
    )
    samples = {}
    for match in trace.finditer(log):
        frame, state, end, enabled, visible, map_rain, spell, count = map(int, match.groups()[:8])
        texture, authored, spacing, speed = match.groups()[8:]
        samples.setdefault(frame, []).append(
            (state, end, enabled, visible, map_rain, spell, count, texture, authored, float(spacing), float(speed))
        )

    for frame in CAPTURE_FRAMES:
        passes = samples.get(frame)
        if not passes:
            raise RuntimeError(f"No snow manager trace for frame {frame}")
        expected_logic = (2, RAIN_END) if FIRST_RAIN_DRAW <= frame < RAIN_END else (0, 0)
        hidden = frame in HIDDEN_FRAMES
        draws_rain = not hidden and FIRST_RAIN_DRAW <= frame < DRY_FRAME
        expected = (*expected_logic, int(draws_rain), int(not hidden), 0, 1)
        if any(sample[:6] != expected for sample in passes):
            raise RuntimeError(f"Frame {frame}: expected weather draw state {expected}, got {passes}")
        if draws_rain and not any(
            count > 0 and texture.lower() == authored.lower() == "exraindrop.tga"
            for _, _, _, _, _, _, count, texture, authored, _, _ in passes
        ):
            raise RuntimeError(f"Frame {frame}: no draw pass submitted the authored rain: {passes}")
        if not draws_rain and any(sample[6] for sample in passes):
            raise RuntimeError(f"Frame {frame}: dry weather submitted precipitation: {passes}")

    ramp = {FIRST_RAIN_DRAW: (30.0, 150.0), 186: (20.0, 250.0), 5436: (20.0, 250.0), DRY_FRAME: (30.0, 50.0)}
    for frame, expected in ramp.items():
        if any(sample[-2:] != expected for sample in samples[frame]):
            raise RuntimeError(f"Frame {frame}: expected spacing and speed {expected}, got {samples[frame]}")
    if any(not (20.0 < sample[-2] < 30.0 and 150.0 < sample[-1] < 250.0) for sample in samples[5487]):
        raise RuntimeError(f"Rain did not ramp out after logic expiry: {samples[5487]}")


def terrain_crop(output, frame):
    with Image.open(output / f"frame-{frame:04d}.png") as image:
        return image.convert("RGB").crop(TERRAIN_CROP)


def changed_pixels(sample, controls):
    low = high = controls[0]
    for control in controls[1:]:
        low = ImageChops.darker(low, control)
        high = ImageChops.lighter(high, control)
    motion = ImageChops.subtract(high, low)
    difference = ImageChops.lighter(ImageChops.subtract(low, sample), ImageChops.subtract(sample, high))
    return sum(max(delta) >= 8 and max(background) <= 2 for delta, background in zip(
        difference.get_flattened_data(), motion.get_flattened_data()
    ))


def check_terrain_brightness(output):
    baseline = terrain_crop(output, PRECAST_FRAME).convert("L").get_flattened_data()
    phases = {
        "before": (PRECAST_FRAME,),
        "cast": (CAST_FRAME,),
        "ramp in": (FIRST_RAIN_DRAW, 66, 96, 136),
        "steady rain (streaks hidden)": HIDDEN_FRAMES,
        "steady rain": (186, 936, RAIN_END - 1),
        "ramp out": (RAIN_END, 5487),
        "after": (DRY_FRAME,),
    }
    rows = [f"Terrain crop {TERRAIN_CROP}, luma on a 0–255 scale", "Phase | Frame | Mean | Median change"]
    deviations = []
    for phase, frames in phases.items():
        for frame in frames:
            luminance = terrain_crop(output, frame).convert("L")
            mean = ImageStat.Stat(luminance).mean[0]
            median_change = median(map(sub, luminance.get_flattened_data(), baseline))
            rows.append(f"{phase} | {frame} | {mean:.2f} | {median_change:+g}")
            if abs(median_change) > 2:
                deviations.append(f"{phase} frame {frame}: {median_change:+g}")
    (output / "terrain-brightness.txt").write_text("\n".join(rows) + "\n")
    if deviations:
        raise RuntimeError("Unexpected terrain lighting change: " + ", ".join(deviations))


def check_captures(output, _frames):
    before, hidden_before, rainy, hidden_after, after = [terrain_crop(output, frame) for frame in range(184, 189)]
    wet = changed_pixels(rainy, (before, hidden_before, hidden_after, after))
    dry = max(
        changed_pixels(hidden_before, (before, hidden_after, after)),
        changed_pixels(hidden_after, (before, hidden_before, after)),
    )
    (output / "rain-crop.txt").write_text(
        f"Crop {TERRAIN_CROP}, pixels at least 8 levels outside the control range\n"
        "Counted only pixels varying by at most 2 levels across the controls\n"
        f"Rainy frame against four hidden captures: {wet}\n"
        f"Largest hidden frame difference against the other hidden captures: {dry}\n"
    )
    if wet <= max(20, dry * 2):
        raise RuntimeError(f"No visible rain in the terrain crop: {wet} vs hidden control {dry}")
    check_terrain_brightness(output)
