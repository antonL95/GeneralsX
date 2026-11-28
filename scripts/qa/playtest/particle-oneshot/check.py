import re

render_env = {"PLAYTEST_PIN_DYNAMIC_LOD": "1"}


def check(log):
    counts = {
        name: int(particles)
        for name, particles in re.findall(
            r"^TEST particle-systems frame=60 name=(Test\w+) systems=1 particles=(\d+) ", log, re.MULTILINE
        )
    }
    expected = {"TestOneShotDelayed", "TestOneShotContinuous", "TestRepeatingDelayed"}
    if set(counts) != expected:
        raise RuntimeError(f"Expected one live system of each kind at frame 60: {counts}")
    if counts["TestOneShotDelayed"] != 3:
        raise RuntimeError(f"A one shot system with a burst delay must emit one burst of 3: {counts}")
    if counts["TestOneShotContinuous"] < 15:
        raise RuntimeError(f"A one shot system without a burst delay must keep emitting: {counts}")
    if not 3 <= counts["TestRepeatingDelayed"] <= 5:
        raise RuntimeError(f"A repeating system with a burst delay must emit repeated bursts: {counts}")
