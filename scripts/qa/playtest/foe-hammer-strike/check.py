import math
import re


render_env = {"GENERALSX_TEST_LIGHTNING": "1", "PLAYTEST_STATIC_CAPTURES": "1"}
POSITION = r"([\d.-]+,[\d.-]+,[\d.-]+)"


def position(value):
    return tuple(map(float, value.split(",")))


def check(log):
    endpoints = {system: (name, position(source), position(target)) for system, name, source, target in re.findall(
        rf"TEST particle-endpoints id=(\d+) (LightningStrike[23]?) source={POSITION} target={POSITION}", log
    )}
    initial_draws = {}
    for system, count, first, last in re.findall(
        rf"TEST lightning frame=\d+ id=(\d+) points=(\d+) first={POSITION} last={POSITION}", log
    ):
        if system in endpoints:
            initial_draws.setdefault(system, (int(count), position(first), position(last)))
    if {round(endpoints[system][2][0]) for system in initial_draws} != {3210, 3240}:
        raise RuntimeError("Expected rendered lightning for both fixture victims")
    for name in ("LightningStrike", "LightningStrike2", "LightningStrike3"):
        rendered = [(system, source, target) for system, (particle, source, target) in endpoints.items()
                    if particle == name and system in initial_draws]
        if not rendered:
            raise RuntimeError(f"{name} never reached the streak renderer")
        for system, source, target in rendered:
            count, first, last = initial_draws[system]
            if math.dist(first, source) > math.dist(last, source):
                first, last = last, first
            if count != 10 or math.dist(first, source) > 3 or math.dist(last, target) > 3:
                raise RuntimeError(f"{name} did not join the sword and victim: {source}, {target}, {first}, {last}")
