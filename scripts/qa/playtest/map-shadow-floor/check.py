import re


render_env = {
    "GENERALSX_TRACE_RETAIL_SHADOWS": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    centers = re.findall(
        r"^\[Retail Shadow\] pass=\d+ game=(\d+) center=\([^,]+,[^,]+,([^()]+)\).*"
        r" minTerrain=450\.000 .* target=00000000 clear=00000000 ",
        log,
        re.MULTILINE,
    )
    views = [float(z) for frame, z in centers if int(frame) >= 20]
    if not views or min(views) < 450:
        raise RuntimeError(f"The 450 unit receiver floor did not reach the shadow projection: {views[:8]}")
