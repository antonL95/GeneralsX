import re


def check(log):
    positions = re.findall(r"TEST frame (16|45|60): camera at ([\d.]+) ([\d.]+) zoom", log)
    expected = [("16", 2280, 1180), ("45", 2780, 1330), ("60", 2580, 1270)]
    if [(frame, float(x), float(y)) for frame, x, y in positions] != expected:
        raise RuntimeError(f"Camera did not resolve both live positions and the explicit point: {positions}")
    projections = re.findall(r"TEST camera subject frame=(26|52) visible=(\d+) view=([\d.-]+),([\d.-]+)", log)
    if [row[0] for row in projections] != ["26", "52"]:
        raise RuntimeError(f"Missing camera projections: {projections}")
    for frame, visible, x, y in projections:
        if visible != "1" or abs(float(x) - 0.5) > 0.07 or abs(float(y) - 0.5) > 0.07:
            raise RuntimeError(f"Subject at frame {frame} projected outside the middle of the view: {x},{y}")
