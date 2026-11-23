import re


def check(log):
    cases = (
        (34, "Command_ForlongTrueHeartTrueFriend", "Bombard"),
        (39, "Command_ForlongTrueHeartTrueFriend", "GenericInvalid"),
        (51, "Command_GondorGandalfIstariLightTiered", "Bombard"),
        (56, "Command_GondorGandalfIstariLightTiered", "GenericInvalid"),
    )
    failures = []
    for frame, button, cursor in cases:
        prefix = f"TEST cursor selected frame={frame} button={button} expected={cursor}"
        if not re.search(rf"^{prefix} actual=\d+ matched=1$", log, re.M):
            actual = next((line for line in log.splitlines() if line.startswith(f"TEST cursor selected frame={frame} ")), "")
            failures.append(f"Expected {cursor} for {button} at {frame}. {actual}")
    if failures:
        raise RuntimeError("\n".join(failures))
