import re


render_env = {
    "GENERALSX_TEST_DRAWTRACE": "WizardBlastCaster,RivendellGandalfTheGrey_Summoned",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    phases = {}
    clips = {}
    for line in log.splitlines():
        phase = re.match(r"TEST ability-phase frame=(\d+) id=\d+ hero=\S+ power=(\S+) phase=(\S+)", line)
        if phase:
            frame, power, name = phase.groups()
            phases.setdefault(power, {})[name] = int(frame)
        draw = re.match(r"TEST draw frame (\d+) id \d+ \S+ module \S+ model \S+ clip (\S+) at ([\d.]+)/\d+ speed ([\d.]+)", line)
        if draw:
            frame, clip, position, speed = draw.groups()
            clips.setdefault(clip.upper(), []).append((int(frame), float(position), float(speed)))

    for power, clip, windup in (
        ("SpecialAbilityWizardBlastGondor", "GUGANDALF_SPCL", 36),
        ("SpecialAbilityIstariLight", "GUGANDALF_SPCH", 66),
        ("SpecialAbilityWordOfPower", "GUGANDALF_SPCK", 54),
    ):
        cast = phases.get(power, {})
        unpack = cast.get("unpack")
        trigger = cast.get("trigger")
        if unpack is None or trigger is None or trigger - unpack != windup:
            raise RuntimeError(f"{power}: expected {windup} frames from unpack to trigger, got {cast}")
        samples = [(frame, position, speed) for name, rows in clips.items() if name.endswith(f".{clip}")
                   for frame, position, speed in rows if unpack + 3 <= frame < trigger]
        if not samples:
            raise RuntimeError(f"{power}: no draw samples for {clip} before trigger")
        if any(speed != 1.0 for _, _, speed in samples):
            raise RuntimeError(f"{power}: expected authored speed during windup, saw {samples[:3]}")
        if max(position for _, position, _ in samples) <= min(position for _, position, _ in samples):
            raise RuntimeError(f"{power}: {clip} did not advance before trigger")
