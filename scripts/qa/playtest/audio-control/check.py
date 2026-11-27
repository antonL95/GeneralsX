import re

DURATION = re.compile(r"AUDIO DURATION handle=(\d+) ms=([\d.]+)")
EVENT = re.compile(
    r"AUDIO TRACE frame=\d+ kind=(start|segment|stop) owner=\d+ handle=(\d+) event=(AudioControl\w+Sound)"
    r"(?: detail=(\w+))?"
)
PCM = re.compile(r"AUDIO PCM SEGMENT event=(AudioControl\w+Sound) handle=(\d+) start=(\d+) length=(\d+)")


def check(log):
    durations = {}
    orders = {"AudioControlRandomSound": [], "AudioControlSequentialSound": [], "AudioControlFiniteSound": []}
    boundaries = {name: [] for name in orders}
    finite_phases = []
    for line in log.splitlines():
        duration = DURATION.match(line)
        if duration:
            durations[duration[1]] = float(duration[2])
        event = EVENT.match(line)
        if event:
            if event[1] != "stop":
                orders[event[3]].append(durations.pop(event[2]))
            if event[3] == "AudioControlFiniteSound":
                finite_phases.append(event[4] or event[1])
        pcm = PCM.match(line)
        if pcm:
            boundaries[pcm[1]].append(tuple(map(int, pcm.groups()[1:])))

    random_order, sequential_order, finite_order = orders.values()
    if len(random_order) < 12 or len(set(random_order)) != 2:
        raise RuntimeError(f"Expected both variants through repeated joins: {random_order}")
    if any(previous == following for previous, following in zip(random_order, random_order[1:])):
        raise RuntimeError(f"Default segment order repeated its immediately preceding clip: {random_order}")
    if (len(sequential_order) < 12 or len(set(sequential_order[:3])) != 3
            or any(clip != sequential_order[index % 3] for index, clip in enumerate(sequential_order))):
        raise RuntimeError(f"Sequential segment order did not cycle through the authored clips: {sequential_order}")
    if finite_phases != ["attack", "sound", "sound", "sound", "decay", "stop"] or finite_order[1:4] != sequential_order[:3]:
        raise RuntimeError(f"Finite event used the wrong segments: {finite_phases}, {finite_order}")
    for name, segments in boundaries.items():
        if len(segments) != len(orders[name]) or not segments or segments[0][1] != 0:
            raise RuntimeError(f"{name} did not start at the beginning of one PCM stream: {segments}")
        for (handle, offset, length), (next_handle, next_offset, _) in zip(segments, segments[1:]):
            if handle != next_handle or offset + length != next_offset:
                raise RuntimeError(f"{name} PCM join at sample {next_offset} should be at {offset + length}")
