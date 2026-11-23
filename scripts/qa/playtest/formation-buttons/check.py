def check(log):
    lines = set(log.splitlines())

    def expect(line):
        if line not in lines:
            raise RuntimeError(f"Missing formation trace: {line}")

    for frame, actor, active in (
        (59, "GondorFountainGuardHorde", 0),
        (62, "GondorFountainGuardHorde", 1),
        (102, "GondorFountainGuardHorde", 0),
        (117, "GondorFountainGuardHorde", 1),
        (127, "GondorFountainGuardHorde", 0),
        (147, "GondorFountainGuardHorde", 1),
        (147, "GondorTowerShieldGuardHorde", 1),
        (172, "GondorFountainGuardHorde", 0),
        (172, "GondorTowerShieldGuardHorde", 0),
    ):
        primary = 0 if active else 15
        alternate = 15 - primary
        expect(f"TEST formation-state {frame} {actor} 0 members=15 primary={primary} alternate={alternate} active={active}")

    for frame in (144, 147, 172):
        expect(f"TEST formation-state {frame} GondorRangerHorde 0 members=12 primary=0 alternate=0 active=0")

    for frame, actor, tray, image in (
        (59, "FountainGuard", 0, "Porcupine"),
        (63, "FountainGuard", 0, "DefaultLine"),
        (103, "FountainGuard", 0, "Porcupine"),
        (113, "FountainGuard", 1, "Porcupine"),
        (118, "FountainGuard", 0, "DefaultLine"),
        (141, "TowerGuard", 0, "Porcupine"),
        (148, "TowerGuard", 0, "DefaultLine"),
    ):
        expect(f"TEST formation-hud {frame} ring=Command_{actor}PorcupineFormation tray={tray} "
               f"image=UCFormation_{image} key=W choice0=W choice1=W double=1")
