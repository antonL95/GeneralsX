render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1"}


def check(log):
    if "[RetailFX] Drew WaterShader PCA_U (wtr_gondor.w3d)" in log:
        return
    water_draws = [line for line in log.splitlines() if line.startswith("[RetailFX] Water reflection")
                   or line.startswith("[RetailFX] Drew WaterShader")]
    raise RuntimeError("The Anduin material did not submit its PCA_U water pass:\n" + "\n".join(water_draws))
