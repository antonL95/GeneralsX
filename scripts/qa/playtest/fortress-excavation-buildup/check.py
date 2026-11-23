import re
import runpy
from pathlib import Path


check_purchases = runpy.run_path(str(Path(__file__).resolve().parents[1] / "fortress_checks.py"))["check_purchases"]
render_env = {
    "PLAYTEST_FACTION": "FactionIsengard",
    "PLAYTEST_STATIC_CAPTURES": "1",
    "GENERALSX_TEST_DRAWTRACE": "IsengardFortressCitadel",
}


def check(log):
    check_purchases(log, "FactionIsengard", "IsengardFortressCitadel", [{
        "button": "Command_PurchaseUpgradeIsengardFortressExcavations",
        "upgrade": "Upgrade_IsengardFortressExcavations",
        "build_frames": "450", "before": "30", "order": "33", "during": "63", "after": "492",
        "art": "flag=FORTRESS_IMPROVEMENT_3",
    }])
    for frame, model, building in ((482, "-", False), (492, "ibfexcav_a", True),
                                   (751, "ibfexcav_a", True), (754, "ibfexcav", False),
                                   (770, "ibfexcav", False)):
        match = re.search(rf"^TEST draw frame {frame} id \d+ IsengardFortressCitadel module ModuleTag_DrawExcavations"
                          r" model (\S+) .* flags (\S*) subobjects ", log, re.MULTILINE)
        if not match or match[1] != model or ("USER_1" in match[2].split(",")) != building:
            raise RuntimeError(f"Unexpected Excavations buildup state at {frame}: {match.groups() if match else None}")
