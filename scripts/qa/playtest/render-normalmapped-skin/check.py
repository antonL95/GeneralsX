import re

render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "GENERALSX_TEST_DRAWTRACE": "MordorTrollCage,PlaytestNormalMappedMulti",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
}


def check(log):
    if not re.search(r"^\[RetailFX\] Drew NormalMapped .* \(MBTROLLPIT_D3\.TROLLPIT\)$",
                     log, re.IGNORECASE | re.MULTILINE):
        draws = [line for line in log.splitlines() if "TROLLPIT" in line.upper() and "[RetailFX]" in line]
        raise RuntimeError("The collapsing Troll Cage did not submit its skinned NormalMapped material:\n"
                           + "\n".join(draws))
    batches = re.findall(r"^\[RetailFX\] NormalMapped batch \(WBCAVE_D2A\.WBCAVE_D2A\) pass=0 "
                         r"material=(\d+) triangles=(\d+) blend=(\d) opacity=([\d.]+)$",
                         log, re.IGNORECASE | re.MULTILINE)
    for blend in ("0", "1"):
        matching = {int(material): int(triangles) for material, triangles, blended, opacity in batches
                    if blended == blend and (blend == "0" or 0 < float(opacity) < 1)}
        if matching != {0: 29, 1: 15}:
            traces = [line for line in log.splitlines() if "PlaytestNormalMappedMulti" in line
                      or "WBCAVE_D2A" in line.upper() or "objupgrade" in line]
            raise RuntimeError(f"Cave debris did not submit both NormalMapped batches (blend={blend}): {batches}\n"
                               + "\n".join(traces[:8] + traces[-8:]))
