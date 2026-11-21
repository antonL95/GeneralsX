"""Prepare and verify the second-sweep power regression sessions on High Pass."""

import argparse
from pathlib import Path
import re


WEATHER = {"Dwarves": (373, 1), "Elves": (282, 1), "Men": (236, 4), "Rohan": (300, 4)}
WEATHER_FRAMES = 900
MAHUD_FRAMES = 600
ALLEGIANCE = """5 player Player_1 - 0 -
6 sweep-setup Player_1 - 0 -
48 objupgrade Player_1 WildSpellBook 0 Upgrade_WildFortressGoblinTownTrigger
64 neutralstate * MoriarGoblinLair 0 -
65 sweep-power Player_1 WildSpellBook 276 Command_SpecialAbilityWildAllegiance_AI 467.9 2926.2
66 sweep-check Player_1 - 276 -
66 neutralstate * MoriarGoblinLair 0 -
66 sweep-unit Player_1 MoriarGoblinLair 9001 Command_ConstructGoblinFighterHordeFromDefectedLair
100 powercrc - - 0 -
1000 sweep-check Player_1 - 9001 -
1010 quit - - 0 -
"""


def prepare(directory):
    angmar = directory / "Angmar.commands"
    commands = [line.split() for line in angmar.read_text().splitlines()]
    casts = [next(int(cmd[0]) for cmd in commands if cmd[1] == "sweep-power" and int(cmd[4]) == case_id)
             for case_id in (270, 271)]
    commands = [cmd for cmd in commands if cmd[1] != "quit"]
    for frame in (casts[0] + MAHUD_FRAMES + 1, casts[1] + MAHUD_FRAMES + 1):
        commands.append([str(frame), "hordelog", "Player_1", "HaradMahudHeadhunterHorde", "0", "-"])
    commands.append([str(casts[1] + MAHUD_FRAMES + 10), "quit", "-", "-", "0", "-"])
    angmar.write_text("".join(" ".join(cmd) + "\n" for cmd in sorted(commands, key=lambda cmd: int(cmd[0]))))

    for side, (case_id, _) in WEATHER.items():
        path = directory / f"{side}.commands"
        commands = [line.split() for line in path.read_text().splitlines()]
        cast = next(int(cmd[0]) for cmd in commands if cmd[1] == "sweep-spell" and int(cmd[4]) == case_id)
        end = cast + WEATHER_FRAMES
        commands = [cmd for cmd in commands if cmd[1] != "quit"]
        for frame in (cast - 1, cast + 1, end - 1, end):
            commands.append([str(frame), "weatherstate", "-", "-", "0", "-"])
            commands.append([str(frame), "powercrc", "-", "-", "0", "-"])
        if side == "Dwarves":
            commands.append(["69", "sweep-spell", "Player_1", "DwarvesSpellBook", "9002", "Command_SpellBookDeepDelving"])
            commands.append([str(end + 1), "combatini", "-", "-", "0",
                             "/app/engine/GeneralsX-slot2/scripts/qa/unbound_passive_spell.ini"])
            commands.append([str(end + 2), "sweep-spell", "Player_1", "DwarvesSpellBook", "9003", "Command_SpellBookDeepDelving"])
        commands.append([str(end + 10), "quit", "-", "-", "0", "-"])
        path.write_text("".join(" ".join(cmd) + "\n" for cmd in sorted(commands, key=lambda cmd: int(cmd[0]))))
    (directory / "WildAllegiance.commands").write_text(ALLEGIANCE)


def require(log, pattern):
    match = re.search(pattern, log, re.MULTILINE)
    if not match:
        raise SystemExit(f"Missing evidence for {pattern}")
    print(match[0])
    return match.groups()


def verify(directory):
    wild = (directory / "WildAllegiance.log").read_text(errors="replace")
    lair_id, old_owner = require(wild, r"^TEST neutralstate frame=64 object=MoriarGoblinLair id=(\d+) owner=(\d+) .* cmdset=EmptyCommandSet$")
    new_owner, = require(wild, rf"^TEST neutralstate frame=66 object=MoriarGoblinLair id={lair_id} owner=(\d+) .* cmdset=NeutralGoblinLairCommandSet$")
    if new_owner == old_owner:
        raise SystemExit("Allegiance kept the same owner")
    require(wild, r"^SWEEP 276 power frame=66 recharge=1 ")
    require(wild, rf"^SWEEP 9001 start frame=66 reason=issued eligibility=0 source={lair_id} set=NeutralGoblinLairCommandSet$")
    require(wild, r"^SWEEP 9001 object frame=1000 id=\d+ complete=1 .* members=[1-9]\d* ")

    angmar = (directory / "Angmar.log").read_text(errors="replace")
    casts = []
    for case_id, slot, weapon in ((270, 3, "TwilightsMistress"), (271, 1, "RedSerpentsVenom")):
        cast, _ = require(angmar, rf"^SWEEP {case_id} weapon frame=(\d+) slot={slot} eligible=([1-9]\d*) selected=\2 switched=\2 weapon=MahudHeadhunterBlowpipe{weapon}$")
        casts.append(int(cast))
        require(angmar, rf"^SWEEP {case_id} power frame=\d+ recharge=1 .* synchronousEffect=1$")
    for cast, locked in zip(casts, (1, 0)):
        frame = cast + MAHUD_FRAMES + 1
        modes = re.findall(rf"^TEST horde mode {frame} \d+ .* locked=(\d+)$", angmar, re.MULTILINE)
        if len(modes) != 12 or any(int(mode) != locked for mode in modes):
            raise SystemExit(f"Unexpected Mahud weapon lock state at frame {frame}")
        print(f"All 12 Mahud members report locked={locked} at frame {frame}")

    for side, (case_id, state) in WEATHER.items():
        log = (directory / f"{side}.log").read_text(errors="replace")
        cast_frame, = require(log, rf"^SWEEP {case_id} start frame=(\d+) reason=issued ")
        cast = int(cast_frame)
        end = cast + WEATHER_FRAMES
        for frame, expected, expiry in ((cast - 1, 0, 0), (cast + 1, state, end), (end - 1, state, end), (end, 0, 0)):
            require(log, rf"^TEST weather frame={frame} state={expected} end={expiry}$")

    dwarves = (directory / "Dwarves.log").read_text(errors="replace")
    for case_id in (248, 335):
        require(dwarves, rf"^SWEEP {case_id} weapon frame=\d+ slot=1 eligible=([1-9]\d*) selected=\1 switched=\1 weapon=EreborDwarvenArcherBowPunchThrough$")
    require(dwarves, r"^SWEEP 9002 start frame=69 reason=passive_upgrade_missing .* upgrades=-$")
    require(dwarves, r"^SWEEP 368 start frame=72 reason=passive_upgrade_granted .* upgrades=Upgrade_GrantDwarvenRiches$")
    require(dwarves, r"^SWEEP 9003 start frame=\d+ reason=passive_upgrade_unbound .* upgrades=-$")
    repeated = (directory / "Dwarves-repeat.log").read_text(errors="replace")
    trace = re.compile(r"^(?:SWEEP (?:\d+ (?:start|power|weapon)|fixture)|TEST (?:weather|powercrc)).*$", re.MULTILINE)
    if trace.findall(dwarves) != trace.findall(repeated):
        raise SystemExit("Dwarves power traces or simulation CRCs differ")
    print("Dwarves power traces and simulation CRCs match across both sessions")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "verify"))
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    (prepare if args.action == "prepare" else verify)(args.directory)
