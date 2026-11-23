render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1"}


def check(log):
    if "[RetailFX] Drew GPUParticle Default (CHFire, " not in log:
        raise RuntimeError("The authored fire did not submit through the retail GPU particle pass")
