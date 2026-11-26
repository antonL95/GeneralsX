import csv
from collections import defaultdict
from pathlib import Path
import sys

APP = Path("/app")
sys.path.insert(0, str(APP / "tools"))

from castle_ini import base_path
from sweep_inventory import Graph, buttons, collect, command_sets, fields, sweep_tree

ABILITY_COMMANDS = {
    "SPECIAL_POWER", "SPECIAL_POWER_TOGGLE", "SPECIAL_POWER_CONSTRUCT", "FIRE_WEAPON", "TOGGLE_WEAPONSET",
    "TOGGLE_WEAPON", "SWITCH_WEAPON", "ONE_RING", "BLOODTHIRSTY", "WAKE_AUTO_PICKUP",
}
CASE_FIELDS = "faction unit hero command_set button command options todo".split()

EXTRA_UNITS = {
    "mordor": [("MordorWitchKingOnFellBeast", 1), ("MordorFellBeast", 1), ("AkhorahilFellBeast", 1)],
    "guldur": [("GuldurGwanthaur", 1), ("KhamulFellBeast", 1)],
}

REPORTED_UNITS = [
    ("Isengard", "IsengardSharku", 1),
    ("Isengard", "IsengardExplosiveMine", 0),
    ("Angmar", "HaradCaranLambar", 1),
    ("Angmar", "HaradBurodapan", 1),
    ("Angmar", "ArbiterofZigurHorde", 0),
    ("Angmar", "HaradCultistsofZigurHorde", 0),
    ("Angmar", "HaradWarlord", 0),
]

EXTRA_BUTTONS = {
    "haradwarlord": {
        "command_upgradeharadwarlordtribalaxemen",
        "command_upgradeharadwarlordtribaljavelineers",
        "command_upgradeharadwarlordtribalspearmen",
    },
    "haradcultistsofzigurhorde": {
        "command_purchaseupgradespiritofzigur",
        "command_zigurcultistsspiritofzigur",
    },
}

TODO_BY_BUTTON = {
    ("mordorwitchking", "command_hourofthewitchkingdualbutton"): "1491",
    ("mordorwitchkingonfellbeast", "command_hourofthewitchkingdualbutton"): "1491",
    ("mordorwitchkingonfellbeast", "command_hourofthewitchkingdualfellbeastbutton"): "1491",
    ("mordorfellbeast", "command_specialabilityscreechfellbeast"): "1492",
    ("mordorfellbeast", "command_specialabilityscreech"): "1492",
    ("khamulfellbeast", "command_specialabilityscreechkhamul"): "1492",
    ("haradburodapan", "command_specialabilityhornofkarna"): "1519",
    ("haradburodapan", "command_givenothingback"): "1514",
    ("haradcaranlambar", "command_specialabilitybreakerofstone"): "1514",
    ("arbiterofzigurhorde", "command_zigurarbiterspiritofzigur"): "1515",
    ("haradcultistsofzigurhorde", "command_zigurcultistsspiritofzigur"): "1515",
    ("haradcultistsofzigurhorde", "command_purchaseupgradespiritofzigur"): "1515",
    ("isengardexplosivemine", "command_deploymine"): "1500",
    ("isengardsharku", "command_sharkuflood"): "1495",
}

TODO_BY_UNIT = {
    "guldurgwanthaur": "1511",
    "haradcaranlambar": "1514",
    "haradburodapan": "1514",
    "isengardsharku": "1495",
    "mordormountaintroll": "1489",
    "mordorfellbeast": "1492",
    "akhorahilfellbeast": "1492",
    "khamulfellbeast": "1492",
    "mordormollok": "1493",
    "haradwarlord": "1516",
}


def todo_for(unit_key, button_key):
    return TODO_BY_BUTTON.get((unit_key, button_key)) or TODO_BY_UNIT.get(unit_key, "-")


def emit_unit(graph, writer, side, unit, hero, initial, seen, kits, force):
    unit_key = unit.casefold()
    if unit_key in seen or unit_key not in graph.objects:
        return
    seen.add(unit_key)
    extra = EXTRA_BUTTONS.get(unit_key, set())
    sets = list(command_sets(graph, unit, initial))
    if not sets:
        return
    abilities = {}
    for index, command_set in enumerate(sets):
        for _, button, data in buttons(graph, command_set):
            command = data.get("command", "").upper()
            button_key = button.name.casefold()
            if button_key in extra or (index == 0 and command in ABILITY_COMMANDS):
                options = " ".join(data.get("options", "").split())
                abilities.setdefault(button_key, (command_set, button.name, command, options))
    if not hero and not force and frozenset(abilities) in kits:
        return
    kits.add(frozenset(abilities))
    canonical = graph.objects[unit_key].name
    for button_key, (command_set, button, command, options) in abilities.items():
        writer.writerow((side, canonical, int(hero), command_set, button, command, options, todo_for(unit_key, button_key)))


def main():
    graph = Graph(list(sweep_tree(APP / "game/ini/aotr-loose/data/ini")), {})
    members = defaultdict(list)
    with (APP / "game/analysis/castle-bases/base-castles.tsv").open() as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            members[base_path(row["path"])].append(row["template"])
    rosters = {fields(roster).get("side", "").casefold(): roster for roster in graph.index["PlayerTemplate"].values()
               if fields(roster).get("playableside", "").casefold() == "yes"}
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(CASE_FIELDS)
    requested_sides = {arg.casefold() for arg in sys.argv[1:]}
    seen_global = defaultdict(set)
    for side in sys.argv[1:]:
        if side.casefold() == "reported":
            for rep_side, unit, hero in REPORTED_UNITS:
                if rep_side.casefold() in requested_sides:
                    continue
                roster = rosters[rep_side.casefold()]
                initial = set(fields(roster).get("initialupgrades", "").lower().split())
                emit_unit(graph, writer, rep_side, unit, hero, initial, seen_global[rep_side.casefold()], set(), True)
            continue
        roster = rosters[side.casefold()]
        player = fields(roster)
        initial = set(player.get("initialupgrades", "").lower().split())
        heroes = player.get("buildableheroesmp", "").split() + player.get("buildableringheroesmp", "").split()
        hero_names = {name.casefold() for name in heroes}
        units = [row[5] for row in collect(graph, roster, members) if row[0] == "unit"]
        seen = seen_global[side.casefold()]
        kits = set()
        for unit in [*heroes, *units]:
            unit_key = unit.casefold()
            force = unit_key in TODO_BY_UNIT or any(u == unit_key for u, _ in TODO_BY_BUTTON)
            emit_unit(graph, writer, side, unit, unit_key in hero_names, initial, seen, kits, force)
        for unit, hero in EXTRA_UNITS.get(side.casefold(), ()):
            emit_unit(graph, writer, side, unit, hero, initial, seen, kits, True)


if __name__ == "__main__":
    main()
