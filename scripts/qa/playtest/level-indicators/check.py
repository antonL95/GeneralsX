import re


def check(log):
    expected_ranks = (
        (106, "GondorPippin", 1),
        (133, "GondorPippin", 4),
        (136, "LamedonClansmenHorde", 1),
        (165, "LamedonClansmenHorde", 2),
        (191, "LamedonClansmenHorde", 5),
        (196, "MordorAttackTroll", 2),
    )
    for frame, template, rank in expected_ranks:
        pattern = rf"^TEST xp-state frame={frame} object=\d+ {template} rank={rank} "
        if not re.search(pattern, log, re.MULTILINE):
            raise RuntimeError(f"Frame {frame} did not show {template} at rank {rank}")

    if not re.search(r"^TEST horde anchor 165 \d+ count 16 selected 0$", log, re.MULTILINE):
        raise RuntimeError("Expected 15 Lamedon Clansmen and their banner carrier")
