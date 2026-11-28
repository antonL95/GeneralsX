import re

render_env = {
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    if "[RetailFX] Drew WaterShader PCA_U (wtr_gondor.w3d)" in log.splitlines():
        raise RuntimeError("Anduin drew PCA_U reflection pass even though ReflectionOn = No")
    if "[RetailFX] Drew WaterShader BumpSpecular_M (wtr_gondor.w3d)" not in log.splitlines():
        raise RuntimeError("Anduin did not fall back to BumpSpecular_M when ReflectionOn = No")
    matches = re.findall(
        r"^\[RetailFX\] Water reflection .* source=(\d+) ",
        log,
        re.MULTILINE,
    )
    if not matches or matches[0] != "0":
        raise RuntimeError(f"Expected water reflection source=0 when ReflectionOn = No, got {matches}")
