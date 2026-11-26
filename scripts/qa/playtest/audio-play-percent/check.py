import re


def check(log):
    requests = set(re.findall(r"kind=sound-queue owner=\d+ handle=\d+ event=(AudioPlayPercent\w+)\b", log))
    if requests != {"AudioPlayPercentAlways", "AudioPlayPercentDefault"}:
        raise RuntimeError(f"Unexpected PlayPercent requests: {sorted(requests)}")
