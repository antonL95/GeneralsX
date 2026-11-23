import re
from pathlib import Path

from PIL import Image, ImageChops


render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_WEATHER_DRAW": "1",
}
render_variants = {"weather-hidden": {"PLAYTEST_HIDE_WEATHER": "1"}}

CAST_FRAME = 37
FIRST_RAIN_DRAW = CAST_FRAME + 1
RAIN_END = CAST_FRAME + 180 * 30
DRY_FRAME = RAIN_END + 103
TERRAIN_CROP = (780, 45, 1240, 295)
CAPTURE_FRAMES = tuple(map(int, Path(__file__).with_name("frames.txt").read_text().split()))


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

    hidden = "TEST playtest weather hidden" in log
    for frame in CAPTURE_FRAMES:
        passes = samples.get(frame)
        if not passes:
            raise RuntimeError(f"No snow manager trace for frame {frame}")
        expected_logic = (2, RAIN_END) if FIRST_RAIN_DRAW <= frame < RAIN_END else (0, 0)
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


def changed_pixels(output, frame):
    name = f"frame-{frame:04d}.png"
    with Image.open(output.parent / name) as a, Image.open(output / name) as b:
        difference = ImageChops.difference(a.convert("RGB").crop(TERRAIN_CROP), b.convert("RGB").crop(TERRAIN_CROP))
    return sum(max(pixel) >= 8 for pixel in difference.get_flattened_data())


def check_captures(output, _frames):
    if output.name != "weather-hidden":
        return
    wet = changed_pixels(output, 186)
    dry = max(changed_pixels(output, frame) for frame in (32, DRY_FRAME))
    (output.parent / "rain-crop.txt").write_text(
        f"Crop {TERRAIN_CROP}, pixels changing by at least 8 channel levels\n"
        f"Rainy frame with and without precipitation: {wet}\n"
        f"Largest dry control difference: {dry}\n"
    )
    if wet <= max(20, dry * 2):
        raise RuntimeError(f"No visible rain in the terrain crop: {wet} vs dry control {dry}")
