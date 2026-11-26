from PIL import Image


render_env = {"GENERALSX_TEST_DRAWTRACE": "DwarvenRoac,IsengardWulfgarCrebain,GondorGwaihirHero_Summoned"}


def check(log):
    for expected in ("DwarvenRoac module ModuleTag_01 model dunroac_skn", "IsengardWulfgarCrebain module ModuleTag_01 model dunarcah_skn"):
        if expected not in log:
            raise RuntimeError(f"Missing expected draw trace: {expected}")


def check_captures(output, frames):
    if 28 not in frames:
        raise RuntimeError("Frame 28 capture is required")
    image = Image.open(output / "frame-0028.png").convert("RGB")
    checks = (
        ("DwarvenRoac", (530, 300, 550, 326), 40),
        ("IsengardWulfgarCrebain", (628, 294, 654, 326), 80),
    )
    failures = []
    for name, box, minimum in checks:
        dark = sum(
            1
            for y in range(box[1], box[3])
            for x in range(box[0], box[2])
            if sum(image.getpixel((x, y))) < 120
        )
        if dark < minimum:
            failures.append(f"{name}: expected at least {minimum} dark bird pixels in {box}, saw {dark}")
    if failures:
        raise RuntimeError("\n".join(failures))
