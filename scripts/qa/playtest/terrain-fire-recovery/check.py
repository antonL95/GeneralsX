import re


def check(log):
    samples = {
        int(frame): (int(fuel), int(rate), int(recovering))
        for frame, fuel, rate, recovering in re.findall(
            r"^TEST terrain fire frame=(\d+) burning=\d+ fuel=(\d+) rate=(\d+) "
            r"health=[\d.]+ recovering=(\d+) x=955 y=2155$",
            log,
            re.MULTILINE,
        )
    }
    expected = {35: (100, 0, 0), 47: (100, 200, 0), 100: (1, 0, 1), 220: (1, 0, 1)}
    if samples != expected:
        raise RuntimeError(f"Expected grass ignition, exhaustion and stable one-fuel recovery, got {samples}")
