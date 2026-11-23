import runpy
from pathlib import Path


fixture = Path(__file__).resolve().parent
setup = runpy.run_path(str(fixture.parent / "fortress_checks.py"))["configure"]
render_env, check = setup(fixture, "FactionElves", "ElvenCitadel")
render_env["GENERALSX_TEST_DRAWTRACE"] += ",GaladhrimWarrior_SlavedGrantMallornBow,BlessedLanternsDebuffLargeRadius,BlessedLanternsDebuffMediumRadius,BlessedLanternsDebuffSmallRadius"
