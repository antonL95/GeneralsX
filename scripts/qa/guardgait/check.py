import math
import statistics
import sys
from collections import defaultdict

GUARDS = ("GondorTowerShieldGuard", "GondorFountainGuard")
CAVALRY_FRAMES = dict(zip(("gucavalry_runa", "gucavalry_runb", "gucavalry_runc", "gucavalry_rund"), (16, 17, 17, 17)))

def check(path):
    previous = {}
    members = defaultdict(dict)
    anchors = defaultdict(dict)
    guard_rates = defaultdict(set)
    excess = defaultdict(list)
    cavalry_errors = []
    with open(path, encoding="utf-8") as trace:
        for line in trace:
            _, frame, unit, number, x, y, _, flags, clip, _, rate, deadline, distance, velocity = line.split()
            frame, x, y, rate = int(frame), float(x), float(y), float(rate)
            distance, velocity = float(distance), float(velocity)
            if unit.endswith("Horde"):
                anchors[unit.removesuffix("Horde")][frame] = (x, y, int(deadline))
                continue
            last = previous.get(number)
            previous[number] = (frame, x, y)
            if flags[0] != "1":
                continue
            step = math.hypot(x - last[1], y - last[2]) if last and last[0] == frame - 1 else None
            if unit == "ArnorCavalry":
                if clip in CAVALRY_FRAMES and step is not None:
                    assert distance == 35, (frame, clip, distance)
                    cavalry_errors.append(abs(rate - step * CAVALRY_FRAMES[clip] / distance))
                continue
            if flags[1] == "1":
                expected = "gutwrgrd_wlka" if flags[2] == "1" else "gutwrgrd_wlkb"
                assert clip == expected, (frame, unit, flags, clip)
                members[unit, frame][number] = flags[2]
            else:
                assert clip in ("gutwrgrd_runa", "gutwrgrd_runb"), (frame, unit, clip)
            assert distance == 0, (frame, unit, clip, distance)
            guard_rates[unit].add(rate)
            if step is not None:
                excess[unit].append(max(0, (step - velocity) * 30))

    for guard in GUARDS:
        assert any(sum(flag == "1" for flag in flags.values()) == 10 and
                   sum(flag == "0" for flag in flags.values()) == 5
                   for (unit, _), flags in members.items() if unit == guard), guard
        print(f"{guard} rate range {min(guard_rates[guard]):.3f}–{max(guard_rates[guard]):.3f}, "
              f"samples above velocity by >1 unit/s: {sum(value > 1 for value in excess[guard])}/{len(excess[guard])}")
        assert guard_rates[guard] == {1.0}, (guard, guard_rates[guard])
        first_x, first_y, deadline = anchors[guard][232]
        held_x, held_y, _ = anchors[guard][236]
        resumed_x, resumed_y, _ = anchors[guard][322]
        assert deadline > 236 and math.hypot(held_x - first_x, held_y - first_y) < 1, (guard, deadline)
        assert math.hypot(resumed_x - held_x, resumed_y - held_y) > 10, guard
    assert len(cavalry_errors) >= 20, len(cavalry_errors)
    print(f"Arnor cavalry Distance=35, samples={len(cavalry_errors)}, median rate error={statistics.median(cavalry_errors):.3f}")
    assert statistics.median(cavalry_errors) < 0.005

if __name__ == "__main__":
    check(sys.argv[1])
