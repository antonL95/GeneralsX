import csv
import os
from pathlib import Path
import re


engine_timeout = 2400
ROOT = Path(__file__).resolve().parent
OWNER = "Player_1"
ENEMY = "PlyrCreeps"
LANES = ((3090, 535), (3150, 690), (2260, 1180), (2520, 1010))
HORSE_PREPARATIONS = {"faramir", "gandalf"}
DISMOUNTS = {"Command_ToggleDismountedFaramir", "Command_GandalfShadowfaxDismount"}
FOOT_MOUTH = "MordorMouthOfSauronCommandSet"
FLOOD = "Command_ElessarOathbreakerFlood"
FOCUSED_CASES = {item.strip() for item in os.getenv("HERO_PARITY_CASES", "").split(",") if item.strip()}


def read_cases():
    with (ROOT / "cases.tsv").open() as stream:
        cases = list(csv.DictReader(stream, delimiter="\t"))
    missing = FOCUSED_CASES - {case["id"] for case in cases}
    if missing:
        raise ValueError("Unknown hero parity cases " + ", ".join(sorted(missing)))
    return [case for case in cases if not FOCUSED_CASES or case["id"] in FOCUSED_CASES]


def prepare_power(case):
    if case["set"] == FOOT_MOUTH:
        return "SpecialAbilityToggleMountedTwo"
    if (case["name"] == "Command_SpecialAbilityGothmogOrcSummon"
            or case["prep"] in HORSE_PREPARATIONS or chill_of_angmar(case)):
        return "SpecialAbilityToggleMounted"
    return None


def chill_of_angmar(case):
    return case["hero"] == "MordorWitchKing" and case["name"] == "Command_HourOfTheWitchKingDualButton"


def lead_time(case):
    if chill_of_angmar(case):
        return 5550
    if prepare_power(case):
        return 172 if case["name"] in DISMOUNTS or case["prep"] not in HORSE_PREPARATIONS else 112
    return 62 if case["samples"] else 38


def positions():
    end = 44
    for case in read_cases():
        cast = end + lead_time(case)
        yield cast, case
        end = cast + int(case["window"])


def render_windows():
    cases = list(positions())
    intervals = [
        f"{cast - 12}:{cast + max(map(int, case['samples'].split(',')))}"
        for cast, case in cases if case["samples"]
    ]
    cast, last = cases[-1]
    end = cast + int(last["window"])
    intervals.append(f"{end}:{end + 1}")
    return " ".join(intervals)


render_env = {
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
    "PLAYTEST_FIXED_UPDATE_FPS": "30",
    "PLAYTEST_RENDER_WINDOWS": render_windows(),
}


def target_template(case):
    if case["name"] == "Command_SpecialAbilityGothmogSiegeMaster":
        return "MordorCatapult"
    if re.search("CaptureBuilding|Structure|ShakeFoundation|OpenInTheName|DeepRot|ShagratTowersCaptainLevel3|Sapper", case["name"]):
        return "HeroParityStructure"
    if re.search("Sacrifice|TrueHeartTrueFriend|Istari|FaramirWound|WoundArrow|FellStinger|BlackDart|Assassin|KingsFavor|MOSLieutenant|Gorbag(?:Bleed|Loyalty)",
                 case["name"]):
        return "HeroParityHero"
    return "HeroParityInfantry"


def extra_setup(case):
    upgrades = []
    if case["power"] == "SpecialAbilityIstariLight":
        upgrades.append("Upgrade_GandalfWhite")
    if case["name"].endswith("_AI"):
        upgrades.append("Upgrade_ObjectUnderAIControl")
    if case["hero"] == "MordorWitchKing" and case["name"] == "Command_WitchKingToggleMounted":
        upgrades.append("Upgrade_DwarvenMithrilMail")
    if chill_of_angmar(case):
        upgrades.append("Upgrade_ObjectLevel1")
    return upgrades


def scenario_text():
    commands, cameras, frames = [], [], []

    def add(frame, action, hero="-", payload="-", player=OWNER, ordinal=0):
        commands.append(f"{frame} {action} {player} {hero} {ordinal} {payload}")

    add(10, "sweep-setup")
    add(20, "create", "HeroParityCamera", "2450,1080,0")
    for cast, case in positions():
        spawn_hero, button, kind = (case[name] for name in ("hero", "name", "kind"))
        hero = "CreateAHeroScaled" if spawn_hero == "CreateAHero" and case["set"] == "Wraith" else spawn_hero
        case_id = int(case["id"])
        x, y = LANES[(case_id - 1) % len(LANES)]
        clear_ground = button.startswith("Command_FaramirAmbushAbility") or button == "Command_SpecialAbilityFaramirSummonLeadership"
        if button == FLOOD:
            x, y = (3090, 850)
            clear_ground = True
        elif clear_ground:
            x, y = LANES[0]
        elif case["samples"]:
            x, y = (3100, 1220)
        target_offset = {FLOOD: 130, "Command_CaptureBuilding": 30}.get(button, 88)
        target_x = x + target_offset
        end = cast + int(case["window"])
        setup = cast - lead_time(case)
        add(setup + 1, "hero-parity-begin")
        if case["profile"]:
            add(setup + 2, "hero-parity-profile", payload=str(ROOT / "cases.tsv"), ordinal=case_id)
        add(setup + 3, "create", spawn_hero, f"{x},{y},0")
        if case["profile"]:
            add(setup + 9, "hero-parity-button", hero, button)
        add(setup + 10, "pw-rank", hero, case["rank"])
        for upgrade in extra_setup(case):
            add(setup + 10, "objupgrade", hero, upgrade)
        prep = prepare_power(case)
        if prep:
            add(setup + (13 if case["prep"] in HORSE_PREPARATIONS else 24), "power", hero, prep)
        elif case["prep"] == "bow-spear":
            add(setup + 13, "hero-parity-toggle", hero, "Command_ToggleBeregondWeapon")
        ally_target = "NEED_TARGET_ALLY_OBJECT" in case["opts"]
        enemy_target = "NEED_TARGET_ENEMY_OBJECT" in case["opts"] or "NEED_TARGET_NEUTRAL_OBJECT" in case["opts"]
        if kind == "shield":
            enemy = "MordorCaveTroll"
            add(cast - 12, "create", enemy, f"{x + 54},{y},180", ENEMY)
        elif kind == "power" and not clear_ground and (not ally_target or enemy_target):
            enemy = target_template(case)
            enemy_x = target_x + (30 if case["name"] == "Command_MollokLeap" else 0)
            add(cast - 5, "create", enemy, f"{enemy_x},{y},180", ENEMY)
        else:
            enemy = None
        ally = target_template(case) if ally_target else "HeroParityInfantry"
        add(cast - 6, "create", ally, f"{x + 36},{y - 30},0")
        add(cast - 4, "damage", ally, "210")
        if case["power"] == "SpecialAbility202ElessarAthelas":
            add(cast - 4, "damage", hero, "175")
        add(cast - 3, "select", hero)
        add(cast - 1, "hero-parity-button", hero, button)
        if kind == "shield":
            add(cast, "attack", enemy, f"{OWNER}:{hero}:0", ENEMY)
        elif kind == "toggle":
            add(cast, "hero-parity-toggle", hero, button)
        elif kind == "power":
            add(cast, "sweep-power", hero, f"{button} {target_x} {y}", ordinal=case_id)
            for offset in (1, 20, end - cast - 3):
                add(cast + offset, "sweep-check", ordinal=case_id)
        add(end - 2, "hero-parity-button", hero, button)
        if kind == "shield":
            add(end - 2, "auto-ability-state", hero, case["power"])
        add(end, "deselect")
        add(end, "hero-parity-clear")
        if case["profile"]:
            add(end, "hero-parity-profile")
        if case["samples"]:
            zoom = 0.75 if re.search("WordOfPower|Flood|Ambush|Wing", button) else 0.55
            cameras.append(f"{cast - 12} {OWNER} HeroParityCamera {x + 35} {y} {zoom}")
            frames.extend(cast + int(offset) for offset in case["samples"].split(","))
    if not cameras:
        cameras.append(f"24 {OWNER} HeroParityCamera 3100 1220 0.6")
    frames.append(end + 1)
    return {
        "camera.txt": "\n".join(cameras),
        "commands.txt": "\n".join(commands),
        "frames.txt": " ".join(map(str, sorted(set(frames)))),
    }


def check(log):
    seen = set(map(int, re.findall(r"^HERO-PARITY (?:button|missing) frame=(\d+)", log, re.MULTILINE)))
    absent = [case["id"] for cast, case in positions() if cast - 1 not in seen]
    if absent:
        raise RuntimeError("Missing hero observations for cases " + ", ".join(absent))
