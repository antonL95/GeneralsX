import csv
import re


def configure(fixture, faction, fortress):
    with (fixture / "purchases.tsv").open() as stream:
        purchases = list(csv.DictReader(stream, delimiter="\t"))

    def check(log):
        check_purchases(log, faction, fortress, purchases)

    return {
        "PLAYTEST_FACTION": faction,
        "PLAYTEST_STATIC_CAPTURES": "1",
        "GENERALSX_TEST_DRAWTRACE": fortress,
    }, check


def check_purchases(log, faction, fortress, purchases, draw=True):
    if not re.search(rf"^TEST player 10 Player_1 template={faction} ", log, re.MULTILINE):
        raise RuntimeError(f"Expected to start as {faction}")

    orders = {}
    states = {}
    draws = {}
    for line in log.splitlines():
        if line.startswith("FORTRESS order "):
            _, _, frame, _, button, upgrade, available, duration = line.split()
            orders[frame] = button, upgrade, available, duration
        elif line.startswith("FORTRESS state "):
            _, _, frame, _, upgrade, drawable, queued, complete = line.split()
            states[frame] = upgrade, drawable.removeprefix("id="), queued, complete
        elif draw and line.startswith("TEST draw frame "):
            match = re.fullmatch(
                rf"TEST draw frame (\d+) id (\d+) {fortress} module \S+ model (\S+) .* flags (\S*) subobjects (.+)",
                line,
            )
            if match:
                frame, drawable, model, flags, subobjects = match.groups()
                draws.setdefault((frame, drawable), set()).update(
                    [f"model={model}", *(f"flag={flag}" for flag in flags.split(",")),
                     *(f"mesh={mesh.split('.', 1)[-1]}" for mesh in subobjects.split(","))]
                )

    failures = []
    for row in purchases:
        upgrade = row["upgrade"]
        expected_order = (row["button"], upgrade, "available=1", f"frames={row['build_frames']}")
        if orders.get(row["order"]) != expected_order:
            failures.append(f"{upgrade} order at {row['order']}: {orders.get(row['order'])}")
        for phase, queued, complete in (("before", 0, 0), ("during", 1, 0), ("after", 0, 1)):
            frame = row[phase]
            if frame == "-":
                continue
            state = states.get(frame)
            if not state or (state[0], *state[2:]) != (upgrade, f"queued={queued}", f"complete={complete}"):
                failures.append(f"{upgrade} {phase} at {frame}: {state}")
            if draw and state:
                art = draws.get((frame, state[1]))
                if not art:
                    failures.append(f"{upgrade} {phase} at {frame}: missing fortress draw")
                elif row["art"] != "-":
                    for expected in row["art"].split(","):
                        if (expected in art) != bool(complete):
                            failures.append(f"{upgrade} {phase} at {frame}: unexpected state for {expected}")
    if failures:
        raise RuntimeError("\n".join(failures))
