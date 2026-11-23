import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "GondorFighter"}


def check(log):
    pattern = (r"^TEST draw frame (\d+) id (\d+) GondorFighter module ModuleTag_01 model \S+"
               r" clip (gunsoldr_idl\w+) at ([\d.]+)/([1-9]\d*) .* flags \S* subobjects")
    previous = {}
    repeats = set()
    switches = set()
    for frame, actor, clip, pose, length in re.findall(pattern, log, re.MULTILINE):
        current = (int(frame), clip, float(pose), int(length))
        before = previous.get(actor)
        if before and current[0] == before[0] + 1 and current[2] < before[2]:
            if before[2] >= before[3] - 1:
                (repeats if clip == before[1] else switches).add(actor)
        previous[actor] = current
    if len(switches) < 5 or not repeats:
        raise RuntimeError(f"Expected finished idle clips to switch or repeat, got {len(switches)} and {len(repeats)} actors")
