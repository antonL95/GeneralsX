import re

engine_timeout = 450


def check(log):
    lines = log.splitlines()
    events = []
    for line in lines:
        match = re.search(r"AUDIO TRACE frame=(\d+) kind=(\S+) owner=(\d+) handle=(\d+) event=(\S+)", line)
        if match:
            frame, kind, owner, handle, event = match.groups()
            events.append((int(frame), kind, owner, handle, event))
    for actor in ("GondorEarnur", "GondorGandalf"):
        match = re.search(rf"TEST frame 12: create {actor} for Player_1 id=(\d+)", log)
        if not match:
            raise RuntimeError(f"Missing {actor} object")
        owner = match.group(1)
        prefix = "EarnurVoice" if actor == "GondorEarnur" else "GandalfVoice"
        voices = [event for event in events if event[2] == owner and event[4].startswith(prefix)]
        if not any(event[1] == "start" for event in voices):
            raise RuntimeError(f"Missing {actor} voice")
        active = set()
        starts_per_frame = {}
        for frame, kind, _, handle, event in voices:
            if kind == "start":
                starts_per_frame[frame] = starts_per_frame.get(frame, 0) + 1
                if starts_per_frame[frame] > 1:
                    raise RuntimeError(f"Multiple {actor} voice starts at frame {frame}")
                active.add(handle)
                if len(active) > 1:
                    raise RuntimeError(f"Overlapping {actor} voices at frame {frame}")
            elif kind == "stop":
                active.discard(handle)
    loop = [event for event in events if event[4] == "AudioLoopProbeSound"]
    starts = [event for event in loop if event[1] == "start"]
    if len(starts) != 1:
        raise RuntimeError(f"Expected one ambient start, saw {len(starts)}")
    if any(event[1] == "stop" and 0 < event[0] < 85 for event in loop):
        raise RuntimeError(f"Ambient stopped while its object was alive: {loop}")
    if not any(event[1] == "segment" and event[0] < 85 for event in loop):
        raise RuntimeError("Ambient never continued after its attack")
    if not any(event[1] == "stop" and event[0] >= 85 for event in loop):
        raise RuntimeError("Ambient did not stop after its object died")
    condition = [event for event in events if event[4] == "AudioConditionProbeSound"]
    if len([event for event in condition if event[1] == "start"]) != 1:
        raise RuntimeError("Condition loop did not start exactly once")
    if any(event[1] == "stop" and 0 < event[0] < 135 for event in condition):
        raise RuntimeError("Condition loop stopped before its condition cleared")
    if not any(event[1] == "stop" and event[0] >= 135 for event in condition):
        raise RuntimeError("Condition loop did not stop after its condition cleared")
    if not any(event[1] == "start" and event[4] == "GandalfVoiceSelect" for event in events):
        raise RuntimeError("Multisound did not start its voice child")
    if not any(event[1] == "start" and event[4] == "HeroVoiceSelect2" for event in events):
        raise RuntimeError("Multisound did not start its second child")
