import re

render_env = {
    "GENERALSX_TEST_DRAWTRACE": "GoblinCaveTroll,DrawEntryProbe,DrawCaptureFlagProbe,DrawOwnerFlagProbe,DrawSpearProbe",
}


def check(log):
    rows = re.findall(
        r"^TEST draw frame (\d+) id \d+ (GoblinCaveTroll|DrawEntryProbe) module \S+ model \S+ clip (\S+)"
        r" at ([\d.]+)/\d+ .* subobjects (\S+)",
        log, re.MULTILINE,
    )
    entry = [(int(frame), subobjects.lower()) for frame, actor, _, _, subobjects in rows if actor == "DrawEntryProbe"]
    toss = [(float(pose), subobjects.lower()) for _, actor, clip, pose, subobjects in rows
            if actor == "GoblinCaveTroll" and clip.lower().endswith("mucavtroll_thra")]
    failures = []
    if not any(36 <= frame < 45 and ".rock:hidden" in subobjects for frame, subobjects in entry):
        failures.append("entry probe did not start with a hidden rock")
    if not any(frame >= 46 and ".rock:shown" in subobjects for frame, subobjects in entry):
        failures.append("OnStateEnter did not reveal the rock")
    if not any(pose < 24 and ".rock:shown" in subobjects for pose, subobjects in toss):
        failures.append("troll did not begin a throw with a visible rock")
    if not any(pose >= 24 and ".rock:hidden" in subobjects for pose, subobjects in toss):
        last_pose = max((pose for pose, _ in toss), default=0)
        failures.append(f"Frame:24 did not hide the rock during the throw (last sampled pose {last_pose})")
    if not re.search(r"^TEST projectile frame=\d+ weapon=TrollRockThrow nugget=\d+ projectile=RockBigTroll ", log, re.MULTILINE):
        failures.append("troll did not fire the rock weapon")
    if not re.search(r"^AUDIO TRACE frame=\d+ kind=sound-queue .* event=DrawEventProbeSound$", log, re.MULTILINE):
        failures.append("draw script sound was not queued")
    for probe, mesh, failure in (
        ("DrawOwnerFlagProbe", "FLAG_MEN", "campaign banner did not use its owner when no capture was active"),
        ("DrawSpearProbe", "SPEAR", "snow troll Frame:1 event did not reveal the spear"),
    ):
        if not re.search(
            rf"^TEST draw frame \d+ id \d+ {probe} .* subobjects \S*\.{mesh}:shown(?:,|$)",
            log, re.MULTILINE | re.IGNORECASE,
        ):
            failures.append(failure)
    if not re.search(
        r"^TEST draw frame \d+ id \d+ DrawCaptureFlagProbe module ModuleTag_DrawMain "
        r".*flags \S*START_CAPTURE\S* subobjects \S*\.FLAG_MEN:shown",
        log, re.MULTILINE,
    ):
        failures.append("capture animation did not show the capturing player's banner")
        lines = log.splitlines()
        failures.extend(line for line in lines if line.startswith("TEST radar capture order"))
        sample = next((line for line in lines if "DrawCaptureFlagProbe module ModuleTag_DrawMain" in line
                       and line.startswith("TEST draw frame") and "START_CAPTURE" in line), None)
        if sample:
            failures.append(sample)
    if failures:
        raise RuntimeError("\n".join(failures))
