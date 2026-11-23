import re


render_env = {
    "GENERALSX_TEST_DRAWTRACE": "RivendellGandalfTheGrey_Summoned",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    cues = re.findall(
        r"TEST animation-fx frame=(\d+) template=RivendellGandalfTheGrey_Summoned "
        r"event=FX_GandalfLightningSword marker=14 clip=([\d.]+)",
        log,
    )
    if len(cues) != 1 or not 14 <= float(cues[0][1]) < 70:
        raise RuntimeError(f"Expected one blade charge inside the authored animation window, got {cues}")
    if "[Retail FXList] ParticleBone name=LightningCharge bone=B_SWORDBONE" not in log:
        raise RuntimeError("The blade charge did not attach to the sword bone")
    stops = re.findall(r"TEST animation-fx-stop frame=(\d+) particle=LightningCharge stop=70 clip=(\d+)", log)
    if len(stops) != 1 or int(stops[0][0]) <= int(cues[0][0]) or int(stops[0][1]) < 70:
        raise RuntimeError(f"Expected the blade emitter to stop at the authored frame 70, got {stops}")
