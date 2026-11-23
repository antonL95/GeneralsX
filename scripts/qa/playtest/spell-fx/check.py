def check(log):
    for frame, power in (
        (45, "SpecialAbility202ElessarAthelas"),
        (160, "SpecialAbilityFaramirAmbush2"),
        (255, "SpecialAbilityHornOfGondor"),
        (365, "SpecialPowerShieldBubbleGondor"),
    ):
        if f"TEST frame {frame}: power {power} button " not in log:
            raise RuntimeError(f"Authored hero button unavailable for {power} at frame {frame}")
    if "TEST frame 165: aim Command_FaramirAmbushAbility2 at point" not in log:
        raise RuntimeError("Faramir's volley did not receive its target location")


render_env = {
    "GENERALSX_TEST_DRAWTRACE": "GondorElessar,GondorFaramir,GondorBoromir,GondorGandalf",
}
