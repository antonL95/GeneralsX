import re


render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
}


def check(log):
    for name in ("GpuTerrainFire", "GpuTerrainSmoke"):
        if not re.search(rf"\[RetailFX\] Drew GPUParticle Default \({name}, [1-9]\d*\)", log):
            raise RuntimeError(f"{name} did not submit terrain-fire particles")
    if "TEST terrain fire frame=35 burning=0 fuel=0 rate=0" not in log:
        raise RuntimeError("Expected an unlit patch before impact")
    if not re.search(r"^TEST terrain fire frame=120 burning=[1-9]\d* ", log, re.MULTILINE):
        raise RuntimeError("Expected burning terrain during the capture")


def check_captures(output, frames):
    from PIL import Image
    import sys

    area = (510, 230, 790, 470)

    def pixels(frame):
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            return image.convert("RGB").crop(area).get_flattened_data()

    unlit = pixels(35)
    warm_pixels = {}
    for frame in frames:
        if frame <= 100:
            continue
        count = 0
        for (r, g, b), (br, bg, bb) in zip(pixels(frame), unlit):
            dr, dg, db = r - br, g - bg, b - bb
            count += dr >= 15 and dg >= 4 and dr - db >= 12 and r > g > b
        warm_pixels[frame] = count
    print(f"Terrain fire crop {area}, warm pixels: {warm_pixels}", file=sys.stderr)
    if sum(count >= 300 for count in warm_pixels.values()) < 2:
        raise RuntimeError("Expected visible flame in the impact crop on at least two frames")
