import re

render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1", "GENERALSX_TEST_DRAWTRACE": "FellowshipBoromir"}


def check(log):
    if not re.search(r"^\[RetailFX\] Drew DefaultW3D Default \(GUBORMORIA_SKN\.(BODY|LEGS)\)$",
                     log, re.IGNORECASE | re.MULTILINE):
        draws = [line for line in log.splitlines() if "FellowshipBoromir" in line or "GUBORMORIA_SKN" in line.upper()]
        raise RuntimeError("Boromir did not submit his skinned DefaultW3D draw:\n" + "\n".join(draws[-8:]))
