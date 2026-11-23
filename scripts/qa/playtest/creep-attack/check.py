def check(log):
    expected = (
        "TEST creep-entry cursor frame=39 attack=1 enter=0",
        "TEST creep-entry state frame=55 dead=0 contained=0 target-container=0 player-attacking=1 ",
        "TEST creep-entry state frame=220 dead=0 contained=0 target-container=0 ",
        "TEST creep-entry state frame=220 dead=1 contained=0 target-container=0 ",
    )
    for line in expected:
        if line not in log:
            raise RuntimeError(f"Missing playtest result: {line}")
