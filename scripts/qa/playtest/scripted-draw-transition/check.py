import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "PlaytestDrawTransitionSoldier"}


def check(log):
    pattern = (r"^TEST draw frame (\d+) id \d+ PlaytestDrawTransitionSoldier module ModuleTag_PlaytestTransition"
               r" model \S+ clip (gunsoldr_\w+) at ([\d.]+)/([1-9]\d*) .* flags (\S*) subobjects")
    samples = [(int(frame), clip, float(pose), int(length), "MOVING" in flags.split(","))
               for frame, clip, pose, length, flags in re.findall(pattern, log, re.MULTILINE)]
    clips = [row[1] for row in samples if row[4]]
    transition, movement, idle = "gunsoldr_atka", "gunsoldr_idlf", "gunsoldr_idlb"
    if transition not in clips or movement not in clips or clips.index(transition) > clips.index(movement):
        observed = sorted({(row[1], row[4]) for row in samples})
        raise RuntimeError(f"Expected the named transition before the movement clip, got {observed}")

    finishing = [row for row in samples if row[1] == movement and not row[4]]
    if len(finishing) < 2 or finishing[-1][2] != finishing[-1][3] - 1:
        raise RuntimeError(f"Expected the movement clip to reach its last pose after MOVING cleared, got {finishing}")
    preceding = [row for row in samples if row[1] == movement and row[4] and row[0] < finishing[0][0]]
    if not preceding or finishing[0][2] < preceding[-1][2]:
        raise RuntimeError(f"Expected the current movement pose to continue, got {preceding[-1:]} before {finishing[0]}")
    if not any(frame > finishing[-1][0] and clip == idle for frame, clip, *_ in samples):
        raise RuntimeError("Expected the idle clip after the movement clip completed")
