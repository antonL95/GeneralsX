import re
import sys
from pathlib import Path


render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "GENERALSX_TEST_WEATHER_DRAW": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check_preset(log):
    rainy = re.findall(r"^TEST weather-draw frame=(\d+) state=2 .*", log, re.MULTILINE)
    if not rainy:
        raise RuntimeError("Expected the spellbook cast to activate rainy weather")

    if "TEST weather-preset state=2 lightning=1 sound=RainStereoLoopMS" not in log:
        raise RuntimeError("The rainy preset did not supply its authored lightning flag and audio event")

    flashes = re.findall(r"^TEST weather-lightning frame=(\d+) duration=10 intensity=0\.250$", log, re.MULTILINE)
    if not flashes or int(flashes[0]) < int(rainy[0]):
        raise RuntimeError("Authored rain must produce the configured lightning pulse after it starts")


def check(log):
    check_preset(log)


if __name__ == "__main__":
    check_preset(Path(sys.argv[1]).read_text(errors="replace"))
