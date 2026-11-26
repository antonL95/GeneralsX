import re
import sys

render_env = {
    "GENERALSX_TRACE_RETAIL_SHADOWS": "1",
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
}


def check(log):
    passes = [
        dict(re.findall(r"(\w+)=(\S+)", line))
        for line in log.splitlines()
        if line.startswith("[Retail Shadow] pass=")
    ]
    if not passes:
        raise RuntimeError("No retail shadow caster passes were recorded")
    if any(entry["casterStage1Mismatches"] != "0" for entry in passes):
        raise RuntimeError("Shadow caster stage 1 mismatch detected")
    if any(
        entry["target"] != "00000000"
        or entry["clear"] != "00000000"
        or entry["failures"] != "0"
        for entry in passes
    ):
        raise RuntimeError("A retail shadow caster pass failed")
    if not any(int(entry["props"]) > 0 for entry in passes):
        raise RuntimeError("No props cast retail shadows")

    receivers = [
        dict(re.findall(r"(\w+)=(\S+)", line))
        for line in log.splitlines()
        if line.startswith("[Retail Shadow] receiver=")
    ]
    if not any(
        entry.get("ready") == "1"
        and (entry.get("authored") == "1" or entry.get("legacy") == "1")
        for entry in receivers
    ):
        raise RuntimeError("Retail terrain shadow receiver was never active")

    exp = re.search(
        r"^TEST particle-systems frame=30 name=ExpBonus systems=(\d+)",
        log,
        re.MULTILINE,
    )
    if not exp or int(exp.group(1)) < 100:
        raise RuntimeError(f"Expected Training Grounds ExpBonus field (>=100 systems), got: {exp and exp.group(0)}")

    for name in ("BarrageExplosion", "BarrageExplosionFire"):
        if f"[RetailFX] Drew GPUParticle Default ({name}," not in log:
            raise RuntimeError(f"Expected native GPU particle pass for {name}")

    print(
        f"render-shadows-particles passed: {len(passes)} shadow passes, {exp.group(1)} ExpBonus systems, native Barrage GPU passes",
        file=sys.stderr,
    )
