import re

render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "GENERALSX_TEST_DRAWTRACE": "FellowshipBoromirTranslucent",
}


def check(log):
    if not re.search(r"^\[RetailFX\] Drew DefaultW3D Default \(GUBORMORIA_SKN\.(BODY|LEGS)\)$",
                     log, re.IGNORECASE | re.MULTILINE):
        draws = [line for line in log.splitlines() if "FellowshipBoromir" in line or "GUBORMORIA_SKN" in line.upper()]
        raise RuntimeError("Translucent Boromir did not draw through DefaultW3D:\n" + "\n".join(draws[-8:]))
    if not re.search(r"^\[RetailFX\] DefaultW3D sorted \(GUBORMORIA_SKN\.(BODY|LEGS)\)",
                     log, re.IGNORECASE | re.MULTILINE):
        draws = [line for line in log.splitlines() if "RetailFX" in line or "GUBORMORIA_SKN" in line.upper()]
        raise RuntimeError("Translucent Boromir did not route through SortingRendererClass:\n" + "\n".join(draws[-8:]))
    opacities = [float(val) for val in re.findall(
        r"^TEST draw-opacity frame (?:120|125|130) id \d+ mesh GUBORMORIA_SKN\.BODY effective ([\d.]+) ",
        log, re.MULTILINE,
    )]
    if not opacities or max(opacities) >= 0.6:
        raise RuntimeError(f"Expected translucent Boromir opacity < 0.6 on frames 120-130, got {opacities}")
