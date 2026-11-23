import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "PlaytestFiringDurationSoldier"}


def check(log):
    shots = [int(frame) for frame in re.findall(
        r"^TEST cooldown frame=(\d+) weapon=PlaytestFiringDurationWeapon .* phase=between ",
        log, re.MULTILINE,
    )]
    if len(shots) < 2:
        raise RuntimeError(f"Expected consecutive shots from the fixture weapon, got {shots}")

    pattern = (r"^TEST draw frame (\d+) id \d+ PlaytestFiringDurationSoldier module ModuleTag_01"
               r" model \S+ clip (gunsoldr_atk\w+) at ([\d.]+)/[1-9]\d* speed ([\d.]+)"
               r" timing end (\d+) flags (\S+)")
    draws = {int(frame): (clip, float(pose), float(speed), int(end), flags.split(","))
             for frame, clip, pose, speed, end, flags in re.findall(pattern, log, re.MULTILINE)}
    first, second = shots[:2]
    initial = draws.get(first + 1)
    before = draws.get(second - 1)
    after = draws.get(second + 1)
    if not all((initial, before, after)) or any("FIRING_A" not in draw[4] for draw in (initial, before, after)):
        raise RuntimeError(f"Expected continuous FIRING_A over shots {first} and {second}, got {initial}, {before}, {after}")
    if not second < initial[3] or before[3] != initial[3]:
        raise RuntimeError(f"Expected shot {second} during the first firing period ending at {initial[3]}")
    if after[3] - second != initial[3] - first:
        raise RuntimeError(f"Shot {second} did not extend the animation deadline, got {after[3]} after {initial[3]}")
    if after[0] != before[0] or after[1] < before[1] or not 0 < after[2] < before[2]:
        raise RuntimeError(f"Expected the active clip to slow and continue on shot {second}, got {before} then {after}")
