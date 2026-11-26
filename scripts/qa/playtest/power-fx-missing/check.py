import re
from operator import sub
from statistics import median

from PIL import Image, ImageStat


render_env = {
    "PLAYTEST_FACTION": "FactionIsengard",
    "GENERALSX_TRACE_RETAIL_PARTICLE_SPAWNS": "1",
    "GENERALSX_TEST_WEATHER_DRAW": "1",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
}


def check(log):
    if not re.search(r"^TEST particle-live frame=75 .* name=Industry ", log, re.MULTILINE):
        raise RuntimeError("SpellBookIndustry did not keep the authored Industry particle system live at frame 75")
    if not re.search(r"^TEST particle-live frame=90 .* name=GuldurCastellanFXReceive_Medium .* count=[1-9]", log, re.MULTILINE):
        raise RuntimeError("Castellan aura symbol did not persist past its initial 60-frame lifetime at frame 90")
    if not re.search(r"^TEST particle-live frame=165 .* name=GuldurCastellanFXReceive_Medium .* count=[1-9]", log, re.MULTILINE):
        raise RuntimeError("Castellan aura symbol did not refresh for frame 165")
    for name in ("AvalancheSnowChunkColumn", "AvalancheSparkleColumn2", "AvalancheDustColumn"):
        if not re.search(rf"^TEST particle-live frame=235 .* name={name} .* count=[1-9]", log, re.MULTILINE):
            raise RuntimeError(f"Wrath of Caradhras did not emit live {name} particles at frame 235")
    if not re.search(r"^TEST weather frame=500 state=1 ", log, re.MULTILINE):
        raise RuntimeError("Cloudy weather power did not enter WEATHER_CLOUDY (1) at frame 500")


def luma_crop(output, frame, box):
    with Image.open(output / f"frame-{frame:04d}.png") as image:
        return image.convert("L").crop(box)


def check_captures(output, _frames):
    taint_box = (480, 220, 800, 480)
    taint_before = luma_crop(output, 35, taint_box).get_flattened_data()
    taint_after = luma_crop(output, 480, taint_box).get_flattened_data()
    taint_delta = median(map(sub, taint_after, taint_before))
    if taint_delta > -10:
        raise RuntimeError(f"Tainted Land did not darken the ground at frame 480 vs 35 (median luma delta {taint_delta:+g})")

    cloudy_box = (480, 180, 960, 480)
    cloudy_before = luma_crop(output, 490, cloudy_box).get_flattened_data()
    cloudy_after = luma_crop(output, 600, cloudy_box).get_flattened_data()
    cloudy_delta = median(map(sub, cloudy_after, cloudy_before))
    if cloudy_delta > -8:
        raise RuntimeError(f"Cloudy weather did not darken the scene at frame 600 vs 490 (median luma delta {cloudy_delta:+g})")

    summary = (
        f"Tainted Land median luma delta (480 vs 35): {taint_delta:+g}\n"
        f"Cloudy weather median luma delta (600 vs 490): {cloudy_delta:+g}\n"
        f"Cloudy mean luma: {ImageStat.Stat(luma_crop(output, 490, cloudy_box)).mean[0]:.2f} -> "
        f"{ImageStat.Stat(luma_crop(output, 600, cloudy_box)).mean[0]:.2f}\n"
    )
    (output / "power-fx-summary.txt").write_text(summary)
