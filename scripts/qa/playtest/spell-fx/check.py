def check(log):
    for frame, power in (
        (160, "SpecialAbilityFaramirAmbush2"),
        (255, "SpecialAbilityHornOfGondor"),
        (365, "SpecialPowerShieldBubbleGondor"),
        (805, "SpecialAbility202ElessarAthelas"),
    ):
        if f"TEST frame {frame}: power {power} button " not in log:
            raise RuntimeError(f"Authored hero button unavailable for {power} at frame {frame}")
    if "TEST frame 165: aim Command_FaramirAmbushAbility2 at point" not in log:
        raise RuntimeError("Faramir's volley did not receive its target location")
    if "[Retail FXList] Sound AragornAthelesEffect " not in log:
        raise RuntimeError("Elessar's Athelas did not play its authored FXList")


render_env = {
    "GENERALSX_TEST_DRAWTRACE": "GondorElessar,GondorFaramir,GondorBoromir,GondorGandalf",
}
