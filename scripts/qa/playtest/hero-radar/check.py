import re


render_env = {"GENERALSX_RETAIL_RADAR_LOG": "1"}


def check(log):
    actor = re.search(r"TEST frame 5: create ArnorHalbarad for Player_1 id=(\d+)", log)
    if not actor:
        raise AssertionError("hero was not created")
    marks = re.findall(rf"RADAR hero frame=(\d+) object={actor[1]} .* halo=6 core=4 layer=overlay", log)
    if not any(5 <= int(frame) < 31 for frame in marks):
        raise AssertionError("lone hero was not drawn with the 6-cell halo and 4-cell core")
    if not any(int(frame) >= 31 for frame in marks):
        raise AssertionError("hero alongside a horde was not drawn")
    if any(int(frame) > 51 for frame in marks):
        raise AssertionError("hero mark survived the hero's death")


def check_captures(output, frames):
    from PIL import Image

    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            minimap = image.convert("RGB").crop((5, 485, 235, 702))
            _, green, blue = minimap.getpixel((156, 152))
            if (blue - green > 30) != (frame < 51):
                raise AssertionError(f"hero center pixel has the wrong color at frame {frame}")
            minimap.save(output / f"minimap-{frame:04d}.png")
