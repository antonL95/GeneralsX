from PIL import Image


render_env = {"PLAYTEST_PIN_DYNAMIC_LOD": "1"}
impact_crop = (582, 278, 698, 378)


def check(log):
    if "aim Command_FaramirAmbushAbility2 at point (3170.0, 535.0)" not in log:
        raise RuntimeError("Hidden Volley did not aim at the impact crop")
    if "(FaramirAmbushDamager2) weapon=FaramirAmbushWeaponReal2 " not in log:
        raise RuntimeError("Hidden Volley did not reach its authored impact")


def warm_pixels(output, frame):
    with Image.open(output / f"frame-{frame:04d}.png") as capture:
        crop = capture.convert("RGB").crop(impact_crop)
        return sum(
            red >= 175 and green >= 105 and red - green >= 22 and green - blue >= 35
            for red, green, blue in crop.getdata()
        )


def check_captures(output, frames):
    counts = {frame: warm_pixels(output, frame) for frame in frames}
    if min(counts[81], counts[111]) < counts[20] + 8:
        raise RuntimeError(f"Missing sustained orange flare in the volley impact crop. "
                           f"Warm pixels by frame: {counts}")
