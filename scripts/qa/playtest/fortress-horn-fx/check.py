import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "DwarvenFortressCitadel,MirkwoodCitadel,RohanFortressCitadel", "PLAYTEST_FIXED_UPDATE_FPS": "30"}


def check(log):
    for event in ("HornoftheGateSound", "GreenwoodFortressHorn", "RohanFortressHorn"):
        if not re.search(rf"AUDIO TRACE frame=\d+ kind=start .* event={event}\b", log):
            raise RuntimeError(f"Missing audio start for {event}")
    for name in ("CastleHornDust", "GreenwoodTauronHorn", "RohanHammerhandHorn"):
        if not re.search(rf"TEST particle-systems frame=\d+ name={name} systems=[1-9]\d* ", log):
            raise RuntimeError(f"Missing particle system {name}")
    for actor, module, flag in (("DwarvenFortressCitadel", "ModuleTag_MainDraw", "USER_8"),
                                ("MirkwoodCitadel", "ModuleTag_Draw", "USER_2"),
                                ("RohanFortressCitadel", "ModuleTag_MainDraw", "USER_2")):
        pattern = rf"TEST draw frame (\d+) id \d+ {actor} module {module} .* flags (\S*) subobjects"
        rows = [(int(frame), set(flags.split(","))) for frame, flags in re.findall(pattern, log)]
        if not any(flag in flags for _, flags in rows):
            raise RuntimeError(f"Missing {flag} for {actor}")
        if not any(frame >= 215 and flag not in flags for frame, flags in rows):
            raise RuntimeError(f"{flag} did not expire for {actor}")
