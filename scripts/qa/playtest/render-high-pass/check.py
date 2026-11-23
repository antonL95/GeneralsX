render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1"}


def check(log):
    expected_draws = (
        "Drew Terrain Default with authored shadows",
        "Drew NormalMapped Default with authored shadows (GBFORTRESS.GBFORTRESS)",
        "Drew PostFX_LookupTable Default (vwheath_vol.tga)",
    )
    for draw in expected_draws:
        if f"[RetailFX] {draw}".casefold() not in log.casefold():
            raise RuntimeError(f"The High Pass capture did not report the authored draw {draw}")
