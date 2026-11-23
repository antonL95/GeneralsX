import re

from PIL import Image, ImageStat


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

    with Image.open(output / "frame-0274.png") as builder, Image.open(output / "frame-0290.png") as farm:
        terrain = (820, 185, 870, 240)
        builder_lightness = ImageStat.Stat(builder.crop(terrain).convert("L")).mean[0]
        farm_lightness = ImageStat.Stat(farm.crop(terrain).convert("L")).mean[0]
        if abs(farm_lightness - builder_lightness) > 5:
            raise RuntimeError("Selecting a finished farm drew a placement disc on the snow")

        outline = (950, 92, 1100, 133)
        blue_count = sum(blue - red > 45 for red, _, blue in farm.crop(outline).convert("RGB").getdata())
        builder_blue = sum(blue - red > 45 for red, _, blue in builder.crop(outline).convert("RGB").getdata())
        if blue_count < 40 or builder_blue >= 10:
            raise RuntimeError("The terrain claim outline must follow the selected farm, then hide for a builder")
