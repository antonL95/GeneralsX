import re


render_env = {"GENERALSX_RETAIL_RADAR_LOG": "1"}


def check(log):
    actor = re.search(r"TEST frame 5: create GondorElessar for Player_1 id=(\d+)", log)
    if not actor:
        raise AssertionError("hero was not created")
    marks = re.findall(rf"RADAR hero frame=(\d+) object={actor[1]} .* halo=6 core=4 layer=overlay", log)
    if not any(5 <= int(frame) < 31 for frame in marks):
        raise AssertionError("lone hero was not drawn with the 6-cell halo and 4-cell core")
    if not any(int(frame) >= 31 for frame in marks):
        raise AssertionError("hero alongside a horde was not drawn")


def check_captures(output, frames):
    from PIL import Image

    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            image.crop((5, 485, 235, 702)).save(output / f"minimap-{frame:04d}.png")
