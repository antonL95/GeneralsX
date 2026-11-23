import re
import runpy
from pathlib import Path


check_purchases = runpy.run_path(str(Path(__file__).resolve().parents[1] / "fortress_checks.py"))["check_purchases"]
render_env = {
    "PLAYTEST_FACTION": "FactionMordor",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_DRAWTRACE": "MordorFortressCitadel,MordorIdleFellbeast",
}


def check(log):
    check_purchases(log, "FactionMordor", "MordorFortressCitadel", [{
        "button": "Command_PurchaseUpgradeMordorFortressFireArrows",
        "upgrade": "Upgrade_MordorFortressFireArrowsTrigger",
        "build_frames": "900", "before": "30", "order": "33", "during": "63", "after": "942", "art": "-",
    }])

    fellbeasts = re.findall(r"^TEST draw frame (\d+) id (\d+) MordorIdleFellbeast ", log, re.MULTILINE)
    ids = {drawable for _, drawable in fellbeasts}
    if len(ids) != 1 or any(int(frame) <= 932 for frame, _ in fellbeasts):
        raise RuntimeError(f"Expected one Fellbeast only after Breeding Ground finished: {fellbeasts[:12]}")

    drawable = ids.pop()
    for frame, lower, upper in ((942, 0.0, 0.07), (1030, 0.36, 0.46), (1200, 0.999, 1.0)):
        samples = re.findall(
            rf"^TEST draw-opacity frame {frame} id {drawable} mesh \S+ effective \S+ ocl ([\d.]+) ",
            log, re.MULTILINE,
        )
        if not samples or any(not lower <= float(opacity) <= upper for opacity in samples):
            raise RuntimeError(f"Expected Fellbeast opacity {lower}–{upper} at frame {frame}, got {samples}")
