import re

render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_DRAWTRACE": "ArnorHalbarad,GondorPippin,ElvenAragorn,RohanMerry,ArnorHalbarad_Summoned",
}

DRAW_ROW = re.compile(
    r"^TEST draw frame (\d+) id \d+ (\S+) module \S+ model \S+ clip (\S+) "
    r"at [\d.]+/\d+ speed ([\d.]+) timing end \d+ flags (\S*) subobjects",
    re.MULTILINE,
)


def check(log):
    if not re.search(r"^TEST frame 7: queue ArnorHalbarad on object \d+$", log, re.MULTILINE):
        raise RuntimeError("Halbarad was not queued at the Rivendell citadel")
    if "TEST player 10 Player_1 template=FactionArnor " not in log:
        raise RuntimeError("Expected to recruit Halbarad as Rivendell")

    samples = {}
    for row in log.splitlines():
        if row.startswith("TEST hero-speed "):
            _, _, frame, hero, _, *fields = row.split()
            samples[(int(frame), hero)] = dict(field.split("=", 1) for field in fields)

    for frame, hero, rank, expected in (
        (1665, "ArnorHalbarad", 1, 55), (1695, "ArnorHalbarad", 1, 55),
        (1666, "GondorPippin", 1, 55), (1696, "GondorPippin", 1, 55),
        (1667, "ElvenAragorn", 1, 55), (1697, "ElvenAragorn", 1, 55),
        (1668, "RohanMerry", 1, 55), (1698, "RohanMerry", 1, 55),
        (1715, "ArnorHalbarad", 5, 55), (1730, "ArnorHalbarad", 5, 55),
        (1755, "ArnorHalbarad_Summoned", 1, 60), (1785, "ArnorHalbarad_Summoned", 1, 60),
    ):
        speed = samples.get((frame, hero), {})
        if (speed.get("rank") != str(rank) or speed.get("set") != "0" or
                abs(float(speed.get("actual", 0)) - expected) > 0.05 or
                float(speed.get("max", 0)) != expected):
            raise RuntimeError(f"{hero} cruise at frame {frame}: {speed}")

    for frame, toggle in ((1753, "1"), (1783, "1"), (1805, "0"), (1835, "1")):
        speed = samples.get((frame, "ArnorHalbarad"), {})
        if speed.get("toggle") != toggle:
            raise RuntimeError(f"Halbarad weapon toggle at frame {frame}: {speed}")

    draws = {
        (int(frame), hero): (clip, float(rate), flags.split(","))
        for frame, hero, clip, rate, flags in DRAW_ROW.findall(log)
        if clip != "-"
    }
    for frame, hero, clip_name, minimum, maximum in (
        (1695, "ArnorHalbarad", "gufaramir_skl.guithil_runa", 1.0, 1.3),
        (1695, "ElvenAragorn", "gufaramir_skl.gufaramira_runb", 1.0, 1.0),
        (1695, "GondorPippin", "rumerry_runc", 1.0, 1.0),
        (1695, "RohanMerry", "rumerry_runc", 1.0, 1.0),
        (1785, "ArnorHalbarad_Summoned", "gufaramir_skl.gufaramir_runa", 1.30, 1.38),
    ):
        clip, rate, flags = draws.get((frame, hero), (None, 0, ()))
        if clip != clip_name or not minimum <= rate <= maximum or "MOVING" not in flags:
            raise RuntimeError(f"{hero} draw at frame {frame}: {clip}, {rate}, {flags}")
