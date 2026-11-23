import re

render_env = {"PLAYTEST_STATIC_CAPTURES": "1", "GENERALSX_TEST_DRAWTRACE": "MenFortressCitadel"}


def check(log):
    upgrades = (
        ("Command_PurchaseUpgradeMenFortressBanners", "Upgrade_MenFortressBanners", (30, 55, 510), "gbfbnnrs"),
        ("Command_PurchaseUpgradeMenFortressHouseOfHealing", "Upgrade_MenFortressHouseOfHealing", (510, 550, 1440), "gbfheal"),
    )
    for button, upgrade, frames, model in upgrades:
        for frame, reason in zip(frames, ("available", "queued", "complete")):
            status = rf"^TEST upgrade_button {frame} {button} {upgrade} reason={reason} "
            if not re.search(status, log, re.MULTILINE):
                raise RuntimeError(f"Expected {upgrade} to be {reason} at frame {frame}")
            draws = re.findall(rf"^TEST draw frame {frame} id \d+ MenFortressCitadel module \S+ model (\S+)",
                               log, re.MULTILINE)
            if not draws or (model in draws) != (reason == "complete"):
                raise RuntimeError(f"Unexpected models at frame {frame} for {upgrade}: {draws}")
