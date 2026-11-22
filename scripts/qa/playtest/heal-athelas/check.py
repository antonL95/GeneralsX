import re

from PIL import Image


def check(log):
    for frame, name in ((127, "AragornAthelas"), (127, "AragornAthelas02"), (188, "SpellHeal")):
        pattern = (rf"TEST particle-systems frame={frame} name={name} "
                   r"systems=[1-9]\d* particles=[1-9]\d*\b")
        if not re.search(pattern, log):
            raise RuntimeError(f"Frame {frame}: expected live particles for {name}")


def check_captures(output, frames):
    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            overhead = image.convert("RGB").crop((600, 150, 900, 320))
            magenta = sum(min(red, blue) - green >= 25
                          for red, green, blue in overhead.get_flattened_data())
        if magenta >= 32:
            raise RuntimeError(f"Frame {frame}: found {magenta} magenta pixels above Elessar")
