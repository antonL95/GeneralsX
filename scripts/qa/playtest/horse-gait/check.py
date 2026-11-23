from collections import defaultdict
from math import hypot, isclose

render_env = {"GENERALSX_TEST_DRAWTRACE": "GondorCavalry", "GENERALSX_TEST_GAITTRACE": "1"}


def tracks_for(log, actor):
    tracks = defaultdict(dict)
    for line in log.splitlines():
        if not line.startswith("TEST gait "):
            continue
        draw = dict(field.split("=", 1) for field in line.split()[2:])
        if draw["actor"] != actor or draw["module"] != "ModuleTag_01":
            continue
        if actor == "GondorGandalf" and "MOUNTED" not in draw["flags"].split(","):
            continue
        tracks[draw["id"]][int(draw["frame"])] = draw
    return tracks


def check_turns(tracks, actor, prefix, authored_blend, turn_rate):
    for direction, suffix in (("LEFT", "tnl1"), ("RIGHT", "tnr1")):
        condition = f"TURN_{direction}_HIGH_SPEED"
        clip = prefix + suffix
        turn_samples = [draw for frames in tracks.values() for draw in frames.values()
                        if condition in draw["logic"].split(",")]
        if not turn_samples or any(draw["clip"] != clip or draw["mode"] != "1" for draw in turn_samples):
            raise RuntimeError(f"Expected {actor} to select the looping {clip} for {condition}")
        if any(not isclose(float(draw["rate"]), turn_rate, abs_tol=0.0001) for draw in turn_samples):
            raise RuntimeError(f"{actor} did not preserve the authored rate for {clip}")
        blended = [draw for draw in turn_samples if 0 < float(draw["blend"]) < 1]
        if not blended:
            raise RuntimeError(f"{actor} switched to {clip} without blending any turn poses")
        if any(float(draw["blend_frames"]) != min(authored_blend, int(draw["length"]) - 1)
               or draw["blend_from"] == "-" for draw in blended):
            raise RuntimeError(f"{actor} applied the wrong blend duration or source for {clip}")

    if not any(draw["clip"] == prefix + "accl" and "ACCELERATE" in draw["requires"].split(",")
               for frames in tracks.values() for draw in frames.values()):
        raise RuntimeError(f"{actor} did not select an authored acceleration clip")


def check(log):
    tracks = tracks_for(log, "GondorCavalry")
    check_turns(tracks, "GondorCavalry", "gucavalry_", 20, 0.8)
    scaled = 0
    for frames in tracks.values():
        for frame, current in frames.items():
            if not 50 <= frame <= 90 or current["requires"] != "MOVING":
                continue
            previous, earlier = frames.get(frame - 1), frames.get(frame - 2)
            if not previous or not earlier or current["clip"] != previous["clip"]:
                continue
            distance = float(current["distance"])
            if distance != 35:
                raise RuntimeError(f"Cavalry did not select the authored distance-scaled run at {frame}")
            travel = hypot(float(previous["x"]) - float(earlier["x"]), float(previous["y"]) - float(earlier["y"]))
            expected_rate = float(current["authored"]) * int(current["length"]) * travel * 30 / (distance * float(current["fps"]))
            if not isclose(float(current["rate"]), expected_rate, abs_tol=0.003):
                raise RuntimeError(f"Cavalry rate did not track its authored travel distance at {frame}")
            scaled += 1
    if not scaled:
        raise RuntimeError("No distance-scaled cavalry run samples")
