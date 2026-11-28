import re


def check(log):
    for frame, power in (
        (160, "SpecialAbilityFaramirAmbush2"),
        (325, "SpecialAbilityHornOfGondor"),
        (430, "SpecialPowerShieldBubbleGondor"),
        (850, "SpecialAbility202ElessarAthelas"),
    ):
        if f"TEST frame {frame}: power {power} button " not in log:
            raise RuntimeError(f"Authored hero button unavailable for {power} at frame {frame}")
    if "TEST frame 165: aim Command_FaramirAmbushAbility2 at point" not in log:
        raise RuntimeError("Faramir's volley did not receive its target location")
    if "[Retail FXList] Sound AragornAthelesEffect " not in log:
        raise RuntimeError("Elessar's Athelas did not play its authored FXList")
    if not re.search(r"^TEST timed weapon frame=\d+ source=\d+\(SpellBookRainOfFire\) weapon=RainOfFirePhaseOneWeapon ", log, re.M):
        raise RuntimeError("Fires of Orodruin did not reach its meteor phase")
    if not re.search(r"^TEST draw frame .* MenLongShotObject_NoWeapon .* clip exarrowvolleyl\.exarrowvolleyl ", log, re.M):
        raise RuntimeError("Faramir's volley did not animate its authored arrow effect")
    if not re.search(r"^TEST timed weapon frame=.*\(FaramirAmbushDamager2\) weapon=FaramirAmbushWeaponReal2 ", log, re.M):
        raise RuntimeError("Faramir's volley did not reach the delayed impact")


engine_timeout = 900
render_env = {
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
    "GENERALSX_TEST_DRAWTRACE": (
        "GondorElessar,GondorFaramir,GondorBoromir,GondorGandalf,MenLongShotObject_NoWeapon,"
        "GondorSentryTower_Independant,GondorArcherSummonedBarricade_Slaved"
    ),
}
