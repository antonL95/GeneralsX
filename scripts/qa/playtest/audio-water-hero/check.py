import math
import re

engine_timeout = 500

EVENT = re.compile(r"AUDIO TRACE frame=(\d+) kind=(\S+) owner=(\d+) handle=(\d+) event=(\S+)(?: detail=(\S+))?")
DURATION = re.compile(r"AUDIO DURATION handle=(\d+) ms=([\d.]+)")
LOOP = "Amb_WaterCreek2Loop"
SEGMENT_SAMPLE_END = 600
FRAME_MS = 1000.0 / 30.0


def check(log):
    events = []
    pending_duration = {}
    for line in log.splitlines():
        duration = DURATION.match(line)
        if duration:
            pending_duration[duration.group(1)] = float(duration.group(2))
            continue
        match = EVENT.match(line)
        if match:
            frame, kind, owner, handle, name, detail = match.groups()
            events.append((int(frame), kind, owner, handle, name, detail, pending_duration.pop(handle, None)))
    water = [event for event in events if event[4] == LOOP and 0 < event[0] < SEGMENT_SAMPLE_END]
    handles = sorted({event[3] for event in water if event[1] == "start"})
    if len(handles) != 2:
        raise RuntimeError(f"Expected two water emitters to start once, saw handles {handles}")
    orders = []
    for handle in handles:
        mine = [event for event in water if event[3] == handle and event[1] in ("start", "segment", "stop")]
        if [event[1] for event in mine].count("start") != 1 or any(event[1] == "stop" for event in mine):
            raise RuntimeError(f"Water loop {handle} restarted or stopped: {[event[:2] for event in mine if event[1] != "segment"]}")
        if mine[0][5] != "attack" or any(event[5] != "sound" for event in mine[1:]):
            raise RuntimeError(f"Water loop {handle} replayed its attack")
        if len(mine) < 20:
            raise RuntimeError(f"Water loop {handle} did not continue: {len(mine)} segments")
        for previous, following in zip(mine, mine[1:]):
            expected = math.ceil(previous[6] / FRAME_MS - 1e-6)
            if following[0] - previous[0] > expected:
                raise RuntimeError(f"Silent frame between water segments at {following[0]}: "
                                   f"{following[0] - previous[0]} frames for a {previous[6]} ms segment")
        orders.append([event[6] for event in mine[1:]])
    if orders[0] == orders[1]:
        raise RuntimeError("Both water emitters played the same clips in lockstep")
    if all(order[i] == order[i + 4] for order in orders for i in range(len(order) - 4)):
        raise RuntimeError("Water segments repeat in a fixed four clip cycle")
    for name in ("PippinVoiceSalute", "PippinVoiceRespawn"):
        starts = [event for event in events if event[4] == name and event[1] == "start"]
        if len(starts) != 1:
            raise RuntimeError(f"Expected exactly one {name}, saw {len(starts)}")
        if starts[0][2] == "0":
            raise RuntimeError(f"{name} has no owning hero")
    recruit = next(event[0] for event in events if event[4] == "PippinVoiceSalute")
    revive = next(event[0] for event in events if event[4] == "PippinVoiceRespawn")
    if not recruit < 800 < 880 < revive:
        raise RuntimeError(f"Recruit line at {recruit} and revive line at {revive} are out of order")
