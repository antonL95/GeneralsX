import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "GondorFighter"}


def check(log):
    pattern = (r"^TEST draw frame (\d+) id (\d+) GondorFighter module \S+ model \S+"
               r" clip (gunsoldr_idl\w+) at ([\d.]+)/[1-9]\d* .* flags \S* subobjects")
    previous = {}
    restarts = set()
    repeats = set()
    for frame, actor, clip, pose in re.findall(pattern, log, re.MULTILINE):
        current = (int(frame), clip, float(pose))
        before = previous.get(actor)
        if before and current[0] == before[0] + 1:
            if current[1] != before[1] or current[2] < before[2]:
                restarts.add(actor)
            if current[1] == before[1] and current[2] < before[2]:
                repeats.add(actor)
        previous[actor] = current
    if len(restarts) < 5 or not repeats:
        raise RuntimeError(f"Expected battalion idle restarts and weighted repeats, got {len(restarts)} and {len(repeats)}")
