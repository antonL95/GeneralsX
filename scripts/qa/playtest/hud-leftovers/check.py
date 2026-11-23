import re

from PIL import Image


def check(log):
    if not re.search(r"TEST frame 225: select object \d+ \(GondorBigTrebuchet_Summoned\)", log):
        raise RuntimeError("High Pass did not select the summoned trebuchet")


def check_captures(output, _frames):
    with Image.open(output / "frame-0244.png") as image:
        portrait_status = image.convert("RGB").crop((245, 651, 307, 682))
        pixels = portrait_status.load()
        gold_width = max(
            sum(red >= 195 and 140 <= green <= 205 and blue <= 80
                for red, green, blue in (pixels[x, y] for x in range(portrait_status.width)))
            for y in range(portrait_status.height)
        )
        if gold_width < 35:
            raise RuntimeError("The selected summon has no gold countdown bar")
