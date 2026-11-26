from PIL import Image, ImageChops


WINDOWS = {
    "MordorFighter": (380, 280, 425, 385),
    "DwarvenZerkerVeteran": (505, 280, 548, 385),
    "ArnorElrond": (645, 270, 695, 385),
    "Wyrm": (960, 20, 1060, 360),
}


def top_and_bottom(mask, box):
    left, top, right, bottom = box
    rows = [y for y in range(top, bottom)
            if sum(1 for x in range(left, right) if mask.getpixel((x, y))) >= 3]
    if not rows:
        raise RuntimeError(f"Nothing drawn in {box}")
    return rows[0], rows[-1]


def check_captures(output, frames):
    background = Image.open(output / "frame-0036.png").convert("RGB")
    drawn = Image.open(output / "frame-0100.png").convert("RGB")
    mask = ImageChops.difference(background, drawn).convert("L").point(lambda value: 255 if value > 40 else 0)
    mask.save(output / "silhouettes.png")
    measured = {name: top_and_bottom(mask, box) for name, box in WINDOWS.items()}
    (output / "heights.txt").write_text("".join(
        f"{name} top={top} bottom={bottom} height={bottom - top + 1}\n"
        for name, (top, bottom) in measured.items()))
    heights = {name: bottom - top + 1 for name, (top, bottom) in measured.items()}
    failures = []
    if measured["Wyrm"][0] < 90:
        failures.append(f"Wyrm head at row {measured['Wyrm'][0]}, drawn larger than its authored scale")
    if heights["DwarvenZerkerVeteran"] < 32:
        failures.append(f"DwarvenZerkerVeteran {heights['DwarvenZerkerVeteran']} px, drawn smaller than its authored scale")
    if heights["ArnorElrond"] > 61:
        failures.append(f"ArnorElrond {heights['ArnorElrond']} px, drawn larger than its authored scale")
    if failures:
        raise RuntimeError("; ".join(failures))
