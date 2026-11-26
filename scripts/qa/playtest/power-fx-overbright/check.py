import re
from PIL import Image


render_env = {
    "PLAYTEST_FACTION": "FactionGuldur",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
    "PLAYTEST_RENDER_WINDOWS": "30:112 244:312",
}

LAND_CROP = (250, 100, 1050, 560)
SUN_CROP = (320, 320, 760, 720)


def check(log):
    taint = re.search(
        r"^TEST particle-live frame=110 .* name=TaintLightShafts .* texture=EXVapor01\.W3D .* alpha=([\d.]+)",
        log,
        re.MULTILINE,
    )
    if not taint:
        raise RuntimeError("TaintLightShafts was not live at frame 110")
    if float(taint.group(1)) >= 0.5:
        raise RuntimeError(f"Unexpected TaintLightShafts alpha at frame 110: {taint.group(1)}")

    sun = re.search(
        r"^TEST particle-live frame=310 .* name=DehydrationFX .* texture=EXCldBrkRays\.W3D .* alpha=([\d.]+)",
        log,
        re.MULTILINE,
    )
    if not sun:
        raise RuntimeError("DehydrationFX was not live at frame 310")
    if float(sun.group(1)) >= 0.05:
        raise RuntimeError(f"Unexpected DehydrationFX alpha at frame 310: {sun.group(1)}")


def check_captures(output, frames):
    with Image.open(output / "frame-0032.png") as pre_land:
        pre_land_px = list(pre_land.convert("RGB").crop(LAND_CROP).get_flattened_data())
    with Image.open(output / "frame-0110.png") as land:
        land_px = list(land.convert("RGB").crop(LAND_CROP).get_flattened_data())

    overbright_green = sum(1 for r, g, b in land_px if g >= 190 and g - b >= 30) / len(land_px)
    mean_g = sum(g for r, g, b in land_px) / len(land_px)
    land_changed = sum(
        1 for a, b in zip(land_px, pre_land_px) if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) >= 8
    ) / len(land_px)

    with Image.open(output / "frame-0310.png") as sun:
        sun_px = list(sun.convert("RGB").crop(SUN_CROP).get_flattened_data())
    near_white = sum(1 for r, g, b in sun_px if r >= 220 and g >= 210 and b >= 180) / len(sun_px)
    mean_luma = sum((r + g + b) / 3.0 for r, g, b in sun_px) / len(sun_px)
    (output / "metrics.txt").write_text(
        f"f110 overbright_green={overbright_green * 100:.2f}% mean_g={mean_g:.1f} changed={land_changed * 100:.2f}%\n"
        f"f310 near_white={near_white * 100:.2f}% mean_luma={mean_luma:.1f}\n"
    )
    if overbright_green >= 0.06:
        raise RuntimeError(
            f"Dol Guldur land power panels are overbright at frame 110: {overbright_green * 100:.2f}% overbright green"
        )
    if land_changed < 0.05:
        raise RuntimeError(f"Dol Guldur land power visuals missing at frame 110: only {land_changed * 100:.2f}% changed")
    if near_white >= 0.01 or mean_luma >= 90.0:
        raise RuntimeError(
            f"Unrelenting Sun shafts are overbright at frame 310: {near_white * 100:.2f}% near white, mean luma {mean_luma:.1f}"
        )
