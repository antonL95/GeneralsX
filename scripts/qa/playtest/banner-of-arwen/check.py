import math
import re


render_env = {
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_DRAWTRACE": "BannerOfArwen",
}


def check(log):
    if "aim Command_HalbaradBannerOfArwen at point (3260.0, 690.0)" not in log:
        raise RuntimeError("Halbarad did not place the banner through the targeted command")
    if not re.search(r"^TEST aura .* owner=\d+\(BannerOfArwen\) bonus=ArwenBannerUnitsBonusAllies targets=[1-9]", log, re.MULTILINE):
        raise RuntimeError("The placed banner did not apply its aura")
    if not re.search(
        r"^TEST draw frame 670 id \d+ BannerOfArwen .* model auarbanner_skn clip auarbanner_skl\.auarbanner_a1 "
        r".* subobjects AUARBANNER_SKN\.SPEARBAN:shown,AUARBANNER_SKN\.BANNER:shown$",
        log, re.MULTILINE,
    ):
        raise RuntimeError("Expected the placed banner to draw both meshes with its idle animation")

    opacity = {int(frame): float(value) for frame, value in re.findall(
        r"^TEST ocl-fade (\d+) 0 -1 ([\d.]+) 3240\.000 690\.000 ", log, re.MULTILINE,
    )}
    first = 87
    duration = 19 * 5
    for frame in (100, 140, 181, 182, 200, 207, 230, 237, 350, 658, 670):
        expected = min((frame - first) / duration, 1.0)
        if not math.isclose(opacity.get(frame, -1.0), expected, abs_tol=1e-6):
            raise RuntimeError(f"Expected banner opacity {expected:.6f} at frame {frame}, got {opacity.get(frame)}")


def check_captures(output, frames):
    from PIL import Image

    def cloth_colors(frame):
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            cloth = image.convert("RGB").crop((465, 245, 570, 309))
        return len({(r, g, b) for r, g, b in cloth.getdata() if g - r >= 18 and b - r >= 20 and r < 45})

    before = cloth_colors(35)
    after = cloth_colors(670)
    if after < before + 30:
        raise RuntimeError(f"Expected the banner's textured cloth after the fade, got {before=} and {after=}")
