import sys
from math import isclose, pi, sin

AMPLITUDE = 0.025
RATE = 0.021
FRAMES_PER_SECOND = 30
POSE_TOLERANCE = 0.0001
render_env = {"PLAYTEST_FIXED_UPDATE_FPS": str(FRAMES_PER_SECOND), "PLAYTEST_STATIC_CAPTURES": "1"}


def track(log, ship, locomotor):
    samples = []
    poses = set()
    for line in log.splitlines():
        if not line.startswith("TEST ship-roll "):
            continue
        row = dict(field.split("=", 1) for field in line.split()[2:])
        if row["ship"] != ship:
            continue
        if row["loco"] != f"{locomotor}:100.000":
            raise RuntimeError(f"{ship} selected {row['loco']}, expected {locomotor}")
        if row["underwater"] != "1" or not isclose(float(row["z"]), float(row["water"]), abs_tol=POSE_TOLERANCE):
            raise RuntimeError(f"{ship} was off the water surface at frame {row['frame']}")
        if not isclose(float(row["logic"]), 0.0, abs_tol=POSE_TOLERANCE):
            raise RuntimeError(f"{ship} rolled in the simulation at frame {row['frame']}")
        poses.add(tuple(row[key] for key in ("x", "y", "z", "yaw")))
        samples.append((int(row["frame"]), float(row["drawn"])))
    if [frame for frame, _ in samples] != list(range(35, 346, 10)):
        initial = next((line for line in log.splitlines() if line.startswith("TEST frame 34: object ")
                        and f" {ship} pos " in line), "")
        raise RuntimeError(f"{ship} has incomplete render samples: {len(samples)}\n{initial}")
    if len(poses) != 1:
        raise RuntimeError(f"{ship} changed its logic pose")
    return samples


def airborne(log):
    rows = [dict(field.split("=", 1) for field in line.split()[2:])
            for line in log.splitlines() if line.startswith("TEST ship-roll ")]
    rows = [row for row in rows if row["ship"] == "MordorFellBeast"]
    if [int(row["frame"]) for row in rows] != list(range(35, 346, 10)):
        raise RuntimeError(f"Airborne control has incomplete render samples: {len(rows)}")
    if any(not row["loco"].startswith("FellBeastLocomotor:") for row in rows):
        raise RuntimeError("Airborne control did not stay on FellBeastLocomotor")
    error = max(abs(float(row["drawn"])) for row in rows)
    if error > POSE_TOLERANCE:
        raise RuntimeError(f"Airborne locomotor with authored aeleron correction rolled by {error:.8f} rad")
    return error


def check(log):
    active = track(log, "ShipRollFixtureAuthored", "TransportShipLocomotor")
    control = track(log, "ShipRollFixtureZero", "ShipRollFixtureZeroLocomotor")
    flat_error = max(abs(roll) for _, roll in control)
    if flat_error > POSE_TOLERANCE:
        raise RuntimeError(f"Zero-amplitude control rolled by {flat_error:.8f} rad")

    lo = min(roll for _, roll in active)
    hi = max(roll for _, roll in active)
    if not (isclose(lo, -AMPLITUDE, abs_tol=0.0005) and isclose(hi, AMPLITUDE, abs_tol=0.0005)):
        raise RuntimeError(f"Expected ±{AMPLITUDE} rad, measured [{lo:.8f}, {hi:.8f}]")

    crossings = []
    for (start, left), (end, right) in zip(active, active[1:]):
        if left < 0 <= right or right <= 0 < left:
            crossing = start - left * (end - start) / (right - left)
            crossings.append((crossing, 1 if right > left else -1))
    if len(crossings) < 2:
        raise RuntimeError("Ship roll did not cross zero twice")
    periods = [2 * (end - start) for (start, _), (end, _) in zip(crossings, crossings[1:])]
    expected = 2 * pi / RATE
    if any(not isclose(period, expected, abs_tol=2.0) for period in periods):
        raise RuntimeError(f"Expected a {expected:.3f}-frame period, measured {periods}")
    origin, direction = crossings[0]
    error = max(abs(roll - direction * AMPLITUDE * sin(RATE * (frame - origin))) for frame, roll in active)
    if error > 0.0005:
        raise RuntimeError(f"Ship roll differs from the authored sine by {error:.8f} rad")
    bird_roll = airborne(log)
    period = sum(periods) / len(periods)
    print(f"Ship roll [{lo:.8f}, {hi:.8f}] rad, period {period:.3f} frames ({period / FRAMES_PER_SECOND:.3f} s), "
          f"zero-amplitude control {flat_error:.8f} rad, "
          f"airborne authored correction {bird_roll:.8f} rad", file=sys.stderr)
