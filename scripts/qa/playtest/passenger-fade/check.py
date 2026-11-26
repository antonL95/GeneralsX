def fields(line):
    return dict(field.split("=", 1) for field in line.split()[2:])


def check(log):
    rows = [fields(line) for line in log.splitlines() if line.startswith("TEST tc ")]
    boards = {row["label"]: row for row in rows if row["action"] == "board"}
    for label in ("beast", "karadd", "chariot"):
        if boards.get(label, {}).get("contained") != "1":
            raise RuntimeError(f"{label} passenger did not board: {[row for row in rows if row["action"] in ("board", "can-contain")]}")
    seats = {(row["label"], int(row["frame"])): row for row in rows if row["action"] == "seat"}
    for label in ("beast", "karadd", "chariot"):
        for frame in (41, 100, 170):
            if (label, frame) not in seats or seats[label, frame]["hidden"] != "0":
                raise RuntimeError(f"{label} passenger is not aboard and visible at frame {frame}: {seats}")
    for label in ("beast", "karadd"):
        for frame in (41, 100, 170):
            if seats[label, frame]["found"] != "1" or seats[label, frame]["error"] != "0":
                raise RuntimeError(f"{label} passenger is not on its authored bone at frame {frame}: {seats[label, frame]}")
    expected = {41: (0, 10), 100: (35, 65), 170: (100, 100)}
    for frame, (low, high) in expected.items():
        opacity = int(seats["beast", frame]["opacity"])
        if not low <= opacity <= high:
            raise RuntimeError(f"Beast passenger opacity {opacity} at frame {frame}, expected {low} to {high}")
        for label in ("karadd", "chariot"):
            if seats[label, frame]["opacity"] != "100":
                raise RuntimeError(f"{label} passenger faded at frame {frame}: {seats[label, frame]}")
    probes = {(row["label"], int(row["frame"])): int(row["opacity"]) for row in rows if row["action"] == "probe"}
    for frame, (low, high) in {181: (0, 15), 195: (35, 65), 215: (100, 100)}.items():
        opacity = probes.get(("beast-out", frame))
        if opacity is None or not low <= opacity <= high:
            raise RuntimeError(f"Beast exit opacity {opacity} at frame {frame}, expected {low} to {high}")
    for label in ("karadd-out", "chariot-out"):
        for frame in (181, 215):
            if probes.get((label, frame)) != 100:
                raise RuntimeError(f"{label} faded on exit at frame {frame}: {probes}")
