import re


render_env = {"PLAYTEST_STATIC_CAPTURES": "1"}


def check(log):
    expected = [
        r"TEST hud-state 36 GondorGandalf .* selected=1 camera=828\.00,545\.00",
        r"TEST hud-state 38 GondorGandalf .* selected=1 camera=1050\.00,590\.00",
        r"TEST hud-state 49 GondorGandalf .* selected=1 camera=1270\.00,810\.00",
        r"TEST hud-state 49 GondorFaramir .* selected=1 camera=1270\.00,810\.00",
        r"TEST hud-group 49 size=2",
        r"TEST hud-state 77 GondorGandalf .* selected=1 camera=1050\.00,590\.00",
        r"TEST hud-state 139 GondorGandalf .* selected=1 camera=1270\.00,810\.00",
    ]
    for pattern in expected:
        if not re.search(pattern + r"$", log, re.MULTILINE):
            observed = "\n".join(line for line in log.splitlines() if line.startswith("TEST hud-"))
            raise AssertionError(f"Missing {pattern}\n{observed}")
