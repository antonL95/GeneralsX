def check(log):
    actors = {}
    tracks = {
        "GondorFountainGuardHorde": [],
        "BlackrootValeArcherHorde": [],
        "GondorKnightHorde": [],
    }
    for line in log.splitlines():
        fields = line.split()
        if fields[:2] == ["TEST", "track"] and fields[4] in tracks:
            actors[fields[3]] = fields[4]
        elif (fields[:2] == ["TEST", "pos"] and fields[3] in actors
              and fields[4] == "0" and int(fields[2]) >= 32):
            x, y = map(float, fields[5:7])
            tracks[actors[fields[3]]].append((int(fields[2]), (x, y)))

    crossings = {}
    for name, track in tracks.items():
        crossing = next((point[1] for _, point in track if point[0] >= 2460), None)
        if crossing is None:
            raise RuntimeError(f"{name} did not cross the choke")
        crossings[name] = crossing

    if not (all(y < 1000 for y in crossings.values()) or all(y > 1000 for y in crossings.values())):
        raise RuntimeError(f"Hordes crossed on opposite sides of the citadel {crossings}")

    for name, track in tracks.items():
        first_frame, start = track[0]
        for frame, point in track[1:]:
            if point[0] >= 2460:
                break
            if point != start:
                first_frame, start = frame, point
            elif frame - first_frame > 30:
                raise RuntimeError(f"{name} stopped before crossing at frame {first_frame}")
