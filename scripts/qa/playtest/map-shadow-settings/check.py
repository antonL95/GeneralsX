import re
import sys
from pathlib import Path


render_env = {
    "GENERALSX_TRACE_RETAIL_SHADOWS": "1",
}


def check_shadow_settings(log):
    passes = re.findall(r"^\[Retail Shadow\] pass=.*$", log, re.MULTILINE)
    active_passes = [line for line in passes if " target=00000000 clear=00000000 " in line]
    if not active_passes:
        raise RuntimeError("Expected at least one successful shadow pass")

    sizes = {int(re.search(r"\bsize=(\d+)", line).group(1)) for line in active_passes}
    if sizes != {512}:
        raise RuntimeError(f"Expected the map's 512-pixel shadow map, got {sorted(sizes)}")

    if any(" view=740.000 minTerrain=10.000 " not in line for line in active_passes):
        raise RuntimeError("The shadow pass did not report the configured view distance and minimum height")


def check(log):
    check_shadow_settings(log)


if __name__ == "__main__":
    check_shadow_settings(Path(sys.argv[1]).read_text(errors="replace"))
