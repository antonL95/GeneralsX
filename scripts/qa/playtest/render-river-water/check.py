render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1"}


def check(log):
    expected = "[RetailFX] Drew RiverWater Default_M (New River) sections=45 opacity=0.588 scroll=0.040 additive=1"
    if expected not in log.splitlines():
        draws = [line for line in log.splitlines() if "[RetailFX]" in line and "RiverWater" in line]
        raise RuntimeError("The Adorn River strip did not submit its authored RiverWater settings:\n"
                           + "\n".join(draws))
