import re
from collections import Counter

from PIL import Image


def check(log):
    for frame, name in ((127, "AragornAthelas"), (127, "AragornAthelas02"), (188, "SpellHeal")):
        pattern = (rf"TEST particle-systems frame={frame} name={name} "
                   r"systems=[1-9]\d* particles=[1-9]\d*\b")
        if not re.search(pattern, log):
            raise RuntimeError(f"Frame {frame}: expected live particles for {name}")
    for page in ("001", "002"):
        name = f"scgameuserinterface512_{page}.tga"
        if f"[TEX_MISSING] '{name}'" not in log:
            raise RuntimeError(f"Healing did not exercise missing atlas page {name}")


def check_captures(output, frames):
    for frame in (100, 104, 120, 121, 122, 123, 124):
        if frame not in frames:
            raise RuntimeError(f"Missing healing atlas checkpoint {frame}")
    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            overhead = image.convert("RGB").crop((600, 150, 900, 320))
            if frame == 120:
                overhead.save(output / "heal-crop.png")
            magenta = Counter((red, green, blue) for red, green, blue in overhead.get_flattened_data()
                              if min(red, blue) - green >= 25)
        solid_pixels = max(magenta.values(), default=0)
        if solid_pixels >= 32 * 32 // 2:
            raise RuntimeError(f"Frame {frame}: found {solid_pixels} solid magenta pixels above Elessar")
