import re

render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    for field in ("RiverTransparencyMultiplier", "ReflectionPlaneZ", "ReflectionOn"):
        if f"[Retail INI] field '{field}':" in log:
            raise RuntimeError(f"WaterTransparency field {field} was still skipped during INI load")
    if "[RetailFX] Drew WaterShader PCA_U (wtr_gondor.w3d)" not in log.splitlines():
        raise RuntimeError("Anduin standing water did not draw PCA_U with authored ReflectionPlaneZ")
    matches = re.findall(
        r"^\[RetailFX\] Water reflection .* source=(\d+) .* plane=([^\s]+) area=([^\s]+) prepared=(\d+) ",
        log,
        re.MULTILINE,
    )
    if not matches:
        raise RuntimeError("Missing [RetailFX] Water reflection trace line")
    source, plane, area, prepared = matches[0]
    if source != "1" or prepared != "1" or abs(float(plane) - 40.0) > 0.01 or abs(float(area) - 66.0) > 0.01:
        raise RuntimeError(f"Expected reflection plane=40 area=66 source=1 prepared=1, got {matches[0]}")
