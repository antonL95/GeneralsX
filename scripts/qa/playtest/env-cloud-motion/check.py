import re


render_env = {
    "GENERALSX_TEST_WEATHER_DRAW": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    states = re.findall(r"^TEST weather frame=\d+ state=(\d+) ", log, re.MULTILINE)
    if not any(state in ("1", "3") for state in states):
        commands = [line for line in log.splitlines() if "MordorSpellBook" in line or "pkg35-cast" in line]
        raise RuntimeError(f"The Darkness spell did not activate cloudy weather. States {states}. {commands[-6:]}")
    samples = re.findall(
        r"^TEST weather-cloud-motion speed=([^,]+),([^ ]+) uv=([^,]+),([^\n]+)$", log, re.MULTILINE
    )
    if not samples:
        raise RuntimeError("The dark cloud texture did not use its authored angle and scroll speed")
    if any(abs(float(x) + 0.06) > 0.00001 or abs(float(y)) > 0.00001 for x, y, _, _ in samples):
        raise RuntimeError(f"Unexpected dark cloud velocity: {samples[:4]}")
    if len({u for _, _, u, _ in samples}) < 2:
        raise RuntimeError("The dark cloud UV did not advance")
