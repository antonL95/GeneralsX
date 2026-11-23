import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "RivendellGreyCompany"}


def check(log):
    pattern = (r"^TEST draw frame (\d+) id (\d+) RivendellGreyCompany module ModuleTag_01 model \S+"
               r" clip (\S+) at ([\d.]+)/(\d+) .* flags (\S*) subobjects")
    samples = {}
    for frame, actor, clip, pose, length, flags in re.findall(pattern, log, re.MULTILINE):
        samples.setdefault(actor, {})[int(frame)] = (clip, float(pose), int(length), set(flags.split(",")))

    for start, end, clips in (
        (68, 120, {"gufaramir_skl.guithil_runa"}),
        (325, 364, {"gufaramir_skl.gudundan_rund", "gufaramir_skl.gudundan_rune"}),
    ):
        cycling = set()
        diagnostics = []
        for actor, frames in samples.items():
            moving = [(frame, clip, pose, length) for frame, (clip, pose, length, flags) in sorted(frames.items())
                      if start <= frame <= end and "MOVING" in flags]
            if moving:
                diagnostics.append((actor, moving[0], moving[-1]))
            for previous, current in zip(moving, moving[1:]):
                if (current[0] == previous[0] + 1 and current[1] == previous[1]
                        and current[1] in clips and current[3] > 1 and current[2] < previous[2]):
                    cycling.add(actor)
        if len(cycling) < 2:
            raise RuntimeError(f"Expected two Grey Company members to cycle a run clip in {start}–{end}, "
                               f"got {len(cycling)}. First/last moving samples per actor: {diagnostics}")
