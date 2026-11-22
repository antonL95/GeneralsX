import re
import sys


render_env = {"GENERALSX_TRACE_RETAIL_SHADOWS": "1"}


def check(log):
    cast = re.search(r"\[Retail FXList\] DynamicDecal name=SCCRohirrimSummon frame=(\d+)", log)
    if not cast:
        raise RuntimeError("The Rohan Allies cast did not create its summon decal")

    passes = [dict(re.findall(r"(\w+)=(\S+)", line))
              for line in log.splitlines() if line.startswith("[Retail Shadow] pass=")]
    if any(entry["casterStage1Mismatches"] != "0" for entry in passes):
        violation = next(line for line in log.splitlines() if line.startswith("[Retail Shadow] bad caster stage 1 "))
        raise RuntimeError(violation)
    if any(entry["target"] != "00000000" or entry["clear"] != "00000000" or entry["failures"] != "0"
           for entry in passes):
        raise RuntimeError("A shadow caster pass failed to render")

    cast_frame = int(cast[1])
    cutout_frames = {int(entry["game"]) for entry in passes if int(entry["cutouts"]) > 0}
    if not any(frame < cast_frame for frame in cutout_frames) or not any(
            frame + 1 in cutout_frames for frame in cutout_frames if frame >= cast_frame):
        raise RuntimeError("Expected tree cutout draws before the cast and on consecutive frames after it")

    summary = f"Shadow caster stage 1 passed on {len(passes)} passes ({len(cutout_frames)} frames with tree cutouts)"
    print(summary, file=sys.stderr)
