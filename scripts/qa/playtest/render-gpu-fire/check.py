render_env = {"GENERALSX_TRACE_RETAIL_EFFECTS": "1"}


def check(log):
    for emitter in ("CHFire", "GPUFirePatch"):
        if f"[RetailFX] Drew GPUParticle Default ({emitter}, " not in log:
            raise RuntimeError(f"The {emitter} fire did not submit through the retail GPU particle pass")
