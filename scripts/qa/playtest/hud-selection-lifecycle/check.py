import re


render_env = {"PLAYTEST_STATIC_CAPTURES": "1"}


def check(log):
    expected = [
        r"TEST hud-state 97 GondorGandalf portrait=UPGondorGandalfNew rank=2 ",
        r"TEST hud-state 102 GondorGandalf portrait=UPGondorGandalfNew rank=2 ",
        r"TEST hud-state 106 GondorFaramir portrait=UPGondorGandalfNew rank=2 ",
        r"TEST hud-state 111 GondorFaramir portrait=HEFaramirnew rank=3 ",
        r"TEST hud-state 266 MenPorter .* selectable=0 hidden=1 selected=0 ",
        r"TEST hud-state 422 MenPorter .* selectable=0 hidden=1 selected=0 ",
        r"TEST hud-state 1800 MenPorter .* selectable=1 hidden=0 ",
        r"TEST hero 335 Player_1 \d+ GondorFaramir state=revivable alive=0 queued=0 level=3 ",
        r"TEST hero 370 Player_1 \d+ GondorFaramir state=revivable alive=0 queued=1 level=3 ",
        r"TEST hero 4800 Player_1 \d+ GondorFaramir state=recruitable alive=1 queued=0 level=3 ",
    ]
    for pattern in expected:
        if not re.search(pattern, log):
            observed = "\n".join(line for line in log.splitlines() if line.startswith("TEST hud-") or
                                 line.startswith(("TEST hero ", "TEST production ")) and "GondorFaramir" in line)
            raise AssertionError(f"Missing {pattern}\n{observed}")
