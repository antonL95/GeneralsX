import re


render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "GENERALSX_TEST_WEATHER_DRAW": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    starts = re.findall(r"^TEST weather-lightning frame=(\d+) duration=10 intensity=0\.250$", log, re.MULTILINE)
    effects = re.findall(r"^\[Retail FXList\] Sound ThunderStrikesStereo owner=\d+ action=start frame=(\d+)$", log, re.MULTILINE)
    if not effects:
        raise RuntimeError("The authored lightning FX never played")
    if starts != effects:
        raise RuntimeError(f"Expected one authored effect at each lightning start, got {starts=} {effects=}")
