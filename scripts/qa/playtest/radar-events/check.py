import re

from PIL import Image


render_env = {
    "GENERALSX_RETAIL_RADAR_LOG": "1",
    "GENERALSX_RETAIL_EVA_LOG": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    events = [
        (int(frame), export, float(x), float(y), int(lifetime))
        for frame, export, x, y, lifetime in re.findall(
            r"RADAR ping frame=\d+ created=(\d+) type=\d+ export=(\w+) world=([\d.]+),([\d.]+) lifetime=(\d+)",
            log,
        )
    ]
    scripted = [(frame, export) for frame, export, _, _, _ in events if 116 <= frame <= 118]
    if scripted != [(116, "PingInformation"), (118, "PingBeacon")]:
        raise AssertionError(f"scripted radar markers differ: {scripted}")
    if "TEST radar named object frame=117 remembered=1" not in log:
        raise AssertionError("infiltration script did not record the named object's location")
    evidence = "\n".join(
        line for line in log.splitlines()
        if line.startswith("TEST") and any(
            term in line for term in (
                "RadarTest", "CaptureFlag", "capture flag=", "object=Inn", "object=SignalFire",
                "radar damage", "radar capture order", "LoneTower", "RADAR",
            )
        )
    )
    captures = set(re.findall(r"TEST capture flag=\d+ linked=([1-9]\d*) owner=3", log))
    if len(captures) != 2:
        raise AssertionError(f"expected two successful capture orders: {events}\n{evidence}")
    construction = [
        event for event in events
        if event[1] == "PingGeneric" and abs(event[2] - 1900) < 10 and abs(event[3] - 1240) < 10
    ]
    if len(construction) != 1:
        raise AssertionError(f"expected one construction completion event: {events}\n{evidence}")
    if "TEST radar damage frame=72 damage=1.0 dead=0" not in log or not any(
        event[:4] == (72, "PingAttack", 2880.0, 1450.0) for event in events
    ):
        raise AssertionError(f"missing attack event: {events}\n{evidence}")
    if not re.search(r"TEST radar damage frame=110 damage=[\d.]+ dead=1", log):
        raise AssertionError(f"hero death was not exercised\n{evidence}")
    if not re.search(r"TEST level \d+ object \d+ GondorSentryTower_Independant", log):
        raise AssertionError(f"spellbook power failed to create a tower\n{evidence}")
    unexpected = [
        event for event in events
        if event not in construction and event[:2] not in {(72, "PingAttack"), *scripted}
    ]
    if unexpected:
        raise AssertionError(f"unexpected radar events: {unexpected}")
    if any(lifetime != 120 for _, _, _, _, lifetime in events):
        raise AssertionError(f"radar event lifetime differs from four seconds: {events}")


def check_captures(output, frames):
    for frame in frames:
        with Image.open(output / f"frame-{frame:04d}.png") as image:
            image.crop((5, 485, 235, 702)).save(output / f"minimap-{frame:04d}.png")
