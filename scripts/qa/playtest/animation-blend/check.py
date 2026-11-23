render_env = {"GENERALSX_TEST_DRAWTRACE": "PlaytestAnimationBlendSoldier", "GENERALSX_TEST_GAITTRACE": "1"}


def check(log):
    def clip_name(value):
        return value.lower().rsplit(".", 1)[-1]

    samples = {}
    for line in log.splitlines():
        if line.startswith("TEST gait "):
            sample = dict(field.split("=", 1) for field in line.split()[2:])
            if sample["module"] == "ModuleTag_PlaytestAnimationBlend":
                samples[int(sample["frame"])] = sample

    moving = "gunsoldr_idlf"
    replacement = "gunmaarms_atkb"
    for frame in (39, 45, 49, 51, 55):
        sample = samples[frame]
        if clip_name(sample.get("playing", sample["clip"])) != moving:
            raise RuntimeError(f"Frame {frame} interrupted the required blend: {sample}")
    if any(clip_name(sample.get("playing", sample["clip"])) == "gunsoldr_atka" for sample in samples.values()):
        raise RuntimeError("The replaced queued clip entered playback")
    if clip_name(samples[45]["queued"]) != "gunsoldr_atka" or clip_name(samples[49]["queued"]) != replacement:
        raise RuntimeError("The latest request did not replace the queued clip")

    old, new = samples[49], samples[51]
    if (old["model"], new["model"]) != ("guanoriens_skn", "dunbardswd_skn"):
        raise RuntimeError(f"Expected a model swap during the blend: {old}, {new}")
    if not (0 < float(old["blend"]) < float(new["blend"]) < 1 and
            old["blend_frames"] == new["blend_frames"] == "22.000" and
            0 < float(new["pose"]) - float(old["pose"]) < 4):
        raise RuntimeError(f"The new model lost the current clip or blend progress: {old}, {new}")

    resumed = samples[63]
    if (clip_name(resumed["playing"]) != replacement or clip_name(resumed["blend_from"]) != moving or
            resumed["queued"] != "-" or resumed["blend_frames"] != "20.000" or
            not 0 < float(resumed["blend"]) < 1 or float(resumed["pose"]) < 12):
        raise RuntimeError(f"Expected a new blend to the latest queued clip after completion: {resumed}")
