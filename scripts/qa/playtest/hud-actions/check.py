import re


def check(log):
    cameras = {
        int(frame): tuple(map(float, values.split()))
        for frame, values in re.findall(r"TEST hud-camera (\d+) ([^\n]+)", log)
    }
    start, ground, general = (cameras[frame] for frame in (31, 119, 169))
    if abs(ground[2]) > 0.01 or abs(general[2] - 1.0) > 0.01:
        raise AssertionError(f"Camera pitch did not follow the two authored views: {cameras}")
    if abs(ground[3] - 0.7) > 0.01 or abs(general[3] - 1.25) > 0.01:
        raise AssertionError(f"Camera zoom did not follow the two authored views: {cameras}")
    if ground[:2] != start[:2] or general[:2] != start[:2]:
        raise AssertionError(f"Camera views unexpectedly changed the world anchor: {cameras}")

    actual = re.findall(r"TEST hud-notification (\d+) visible=(\d+)", log)
    expected = [("169", "1"), ("173", "0"), ("201", "1"), ("205", "0")]
    if actual != expected:
        raise AssertionError(f"Expected both persistent notifications to stay open until clicked: {actual}")
