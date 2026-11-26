import re


def check(log):
    values = re.search(r"AUDIO VARIATION event=AudioFileShiftProbe pitch=(\S+) volume=(\S+)", log)
    if not values:
        raise RuntimeError("The authored variation event did not reach audio preparation")
    pitch, volume = map(float, values.groups())
    if pitch != 1.2 or not 0.35 <= volume < 1:
        raise RuntimeError(f"Per-file shift did not reach audio preparation: pitch {pitch}, volume {volume}")
