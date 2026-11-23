import re


def check(log):
    power = "SpecialPowerShieldBubbleGondor"
    if f"TEST auto state frame=25 power={power} ready=1 next=5 dead=0 disabled=0 bubble=0" not in log:
        raise RuntimeError("The shield was not ready before combat")
    if not re.search(
        rf"^TEST auto state frame=(78|130) power={power} ready=0 next=\d+ dead=0 disabled=0 bubble=1$",
        log,
        re.M,
    ):
        raise RuntimeError("The shield did not activate during combat")
