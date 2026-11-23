import re


render_env = {
    "PLAYTEST_STATIC_CAPTURES": "1",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "GENERALSX_TRACE_RETAIL_PARTICLE_SPAWNS": "1",
}


def check(log):
    impacts = re.findall(
        r"^TEST impact frame=(\d+) weapon=GondorTrebuchetRockWarheadFlaming .* victim=(\d+) ",
        log, re.MULTILINE,
    )
    if len(impacts) != 2 or impacts[0][1] != "0" or impacts[1][1] == "0":
        raise RuntimeError(f"Expected the upgraded weapon to hit ground and then a unit, got {impacts}")
    for impact, _ in impacts:
        for name in ("FWDragonExplosion", "FWDragonExplosionGlow", "CatapultExplosionSmoke",
                     "CatapultProjectileChunks", "CatapultProjNewExpFire"):
            if not re.search(rf"^\[Retail Particle\] frame={impact} id=\d+ name={name} ", log, re.MULTILINE):
                raise RuntimeError(f"Impact at frame {impact} did not create {name}")
    for sample in (130, 407):
        if not re.search(
            rf"^TEST particle-live frame={sample} .* name=CatapultProjectileChunks .* "
            r"texture=MUCatapult_FP02\.w3d count=[1-9]\d* ",
            log, re.MULTILINE,
        ):
            raise RuntimeError(f"Expected live debris from the authored W3D model at frame {sample}")


def check_captures(output, frames):
    from PIL import Image, ImageFilter

    for frame in (138, 406, 418):
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            crop = image.convert("RGB").crop((450, 55, 850, 460))
        dark = Image.new("L", crop.size)
        dark.putdata([255 if max(pixel) < 16 else 0 for pixel in crop.getdata()])
        if dark.filter(ImageFilter.MinFilter(17)).getbbox():
            raise RuntimeError(f"Found a near-black debris rectangle in the impact at frame {frame}")

    for frame, bounds in ((138, (540, 320, 580, 350)), (406, (535, 232, 575, 270))):
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            fragment = image.convert("RGB").crop(bounds)
        lava_colors = {(r, g, b) for r, g, b in fragment.getdata() if r - g >= 35 and g - b >= 10}
        if len(lava_colors) < 20:
            raise RuntimeError(f"Expected a textured lava fragment beside the plume at frame {frame}")
