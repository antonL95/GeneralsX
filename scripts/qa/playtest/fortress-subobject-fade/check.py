import re


render_env = {
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_DRAWTRACE": "MenFortressCitadel",
}


def check(log):
    states = re.findall(r"^FORTRESS state (\d+) MenFortressCitadel Upgrade_MenFortressBanners id=(\d+) (.+)$",
                        log, re.MULTILINE)
    if [(frame, state) for frame, _, state in states] != [
        ("30", "queued=0 complete=0"), ("63", "queued=1 complete=0"), ("485", "queued=0 complete=1")
    ]:
        raise RuntimeError(f"Expected the authored Banners order to complete at normal research speed: {states}")
    drawable = states[0][1]

    for frame, show, hide in ((63, "hidden", "shown"), (485, "hidden", "shown"),
                              (501, "shown", "shown"), (521, "shown", "hidden")):
        match = re.search(rf"^TEST draw frame {frame} id {drawable} MenFortressCitadel module ModuleTag_MainDraw "
                          r"model gbfortress .* subobjects (.+)$", log, re.MULTILINE)
        if not match or any(f"GBFORTRESS.{mesh}:{visibility}" not in match[1] for mesh, visibility in
                            (("GBFFLAMING", show), ("BLACK", hide))):
            raise RuntimeError(f"Unexpected subobject visibility at frame {frame}: {match[1] if match else None}")

    for mesh in ("GBFFLAMING", "BLACK"):
        samples = re.findall(rf"^TEST draw-opacity frame 501 id {drawable} mesh GBFORTRESS\.{mesh} .* "
                             r"animation ([\d.]+) ", log, re.MULTILINE)
        if not samples or not all(0.38 < float(opacity) < 0.62 for opacity in samples):
            raise RuntimeError(f"Expected partial opacity for {mesh} during the fade: {samples}")
